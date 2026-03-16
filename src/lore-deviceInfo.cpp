#include "lore-deviceInfo.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
/*
 * For macOS location support, compile this file as Objective-C++ (.mm),
 * not plain .cpp, so CoreLocation is available.
 */
#ifdef __OBJC__
#import <Foundation/Foundation.h>
#import <CoreLocation/CoreLocation.h>
#endif
#endif

namespace {

static char *dup_cstr(const std::string &s)
{
	char *out = (char *)malloc(s.size() + 1);
	if (!out)
		return nullptr;

	memcpy(out, s.c_str(), s.size() + 1);
	return out;
}

static std::string json_escape(const std::string &s)
{
	std::ostringstream o;
	for (char c : s) {
		switch (c) {
		case '\"': o << "\\\""; break;
		case '\\': o << "\\\\"; break;
		case '\b': o << "\\b"; break;
		case '\f': o << "\\f"; break;
		case '\n': o << "\\n"; break;
		case '\r': o << "\\r"; break;
		case '\t': o << "\\t"; break;
		default:
			if ((unsigned char)c < 0x20) {
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", c & 0xff);
				o << buf;
			} else {
				o << c;
			}
		}
	}
	return o.str();
}

static std::string trim(const std::string &s)
{
	size_t a = 0;
	size_t b = s.size();

	while (a < b && std::isspace((unsigned char)s[a]))
		++a;
	while (b > a && std::isspace((unsigned char)s[b - 1]))
		--b;

	return s.substr(a, b - a);
}

static bool read_whole_file(const char *path, std::string &out)
{
	std::ifstream f(path, std::ios::in | std::ios::binary);
	if (!f.is_open())
		return false;

	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

static std::string exec_capture(const std::string &cmd)
{
#if defined(_WIN32)
	FILE *pipe = _popen(cmd.c_str(), "r");
#else
	FILE *pipe = popen(cmd.c_str(), "r");
#endif
	if (!pipe)
		return {};

	std::array<char, 4096> buffer{};
	std::string result;

	while (fgets(buffer.data(), (int)buffer.size(), pipe)) {
		result += buffer.data();
	}

#if defined(_WIN32)
	_pclose(pipe);
#else
	pclose(pipe);
#endif

	return trim(result);
}

static std::string make_error_json(const std::string &err)
{
	return std::string("{\"ok\":false,\"error\":\"") + json_escape(err) + "\"}";
}

static std::string make_device_id_json(const std::string &platform,
				       const std::string &id_type,
				       const std::string &device_id)
{
	std::ostringstream os;
	os << "{"
	   << "\"ok\":true,"
	   << "\"platform\":\"" << json_escape(platform) << "\","
	   << "\"idType\":\"" << json_escape(id_type) << "\","
	   << "\"deviceId\":\"" << json_escape(device_id) << "\""
	   << "}";
	return os.str();
}

static std::string detect_location_windows()
{
	const char *ps =
		"powershell -NoProfile -ExecutionPolicy Bypass -Command "
		"\""
		"$ErrorActionPreference='Stop';"
		"Add-Type -AssemblyName System.Runtime.WindowsRuntime;"
		"$null=[Windows.Devices.Geolocation.Geolocator,Windows.Devices.Geolocation,ContentType=WindowsRuntime];"
		"$asTask=[System.WindowsRuntimeSystemExtensions].GetMethods() | "
		"Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 } | "
		"Select-Object -First 1;"
		"function Await($op) { $asTask.MakeGenericMethod($op.GetType().GenericTypeArguments[0]).Invoke($null,@($op)).GetAwaiter().GetResult() }"
		"$status=Await ([Windows.Devices.Geolocation.Geolocator]::RequestAccessAsync());"
		"if ($status.ToString() -ne 'Allowed') { Write-Output '{\\\"ok\\\":false,\\\"error\\\":\\\"Location access not allowed\\\"}'; exit 0 };"
		"$g=New-Object Windows.Devices.Geolocation.Geolocator;"
		"$g.DesiredAccuracy=[Windows.Devices.Geolocation.PositionAccuracy]::High;"
		"$pos=Await ($g.GetGeopositionAsync());"
		"$c=$pos.Coordinate.Point.Position;"
		"$obj=@{"
		" ok=$true;"
		" provider='windows-geolocator';"
		" latitude=$c.Latitude;"
		" longitude=$c.Longitude;"
		" accuracyMeters=$pos.Coordinate.Accuracy;"
		" altitudeMeters=$c.Altitude;"
		" timestamp=$pos.Coordinate.Timestamp.UtcDateTime.ToString('o')"
		"};"
		"$obj | ConvertTo-Json -Compress"
		"\"";

	std::string out = exec_capture(ps);
	if (out.empty())
		return make_error_json("Windows geolocation command failed");
	return out;
}

#ifdef __APPLE__
#ifdef __OBJC__

@interface TLLocationDelegate : NSObject <CLLocationManagerDelegate>
@property(nonatomic, strong) CLLocationManager *manager;
@property(nonatomic, strong) CLLocation *location;
@property(nonatomic, strong) NSError *lastError;
@property(nonatomic, assign) BOOL done;
@end

@implementation TLLocationDelegate
- (void)locationManager:(CLLocationManager *)manager didUpdateLocations:(NSArray<CLLocation *> *)locations
{
	(void)manager;
	if (locations.count > 0) {
		self.location = locations.lastObject;
		self.done = YES;
	}
}

- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error
{
	(void)manager;
	self.lastError = error;
	self.done = YES;
}

- (void)locationManagerDidChangeAuthorization:(CLLocationManager *)manager
{
	CLAuthorizationStatus status = manager.authorizationStatus;
	if (status == kCLAuthorizationStatusDenied ||
	    status == kCLAuthorizationStatusRestricted) {
		self.done = YES;
	}
}
@end

static std::string detect_location_mac(int timeout_ms)
{
	@autoreleasepool {
		if (![CLLocationManager locationServicesEnabled]) {
			return make_error_json("macOS Location Services are disabled");
		}

		TLLocationDelegate *delegate = [TLLocationDelegate new];
		delegate.manager = [CLLocationManager new];
		delegate.manager.delegate = delegate;
		delegate.manager.desiredAccuracy = kCLLocationAccuracyBest;

		if ([delegate.manager respondsToSelector:@selector(requestWhenInUseAuthorization)]) {
			[delegate.manager requestWhenInUseAuthorization];
		}

		[delegate.manager startUpdatingLocation];

		const int sleep_ms = 100;
		int waited = 0;
		while (!delegate.done && waited < timeout_ms) {
			[[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:(sleep_ms / 1000.0)]];
			waited += sleep_ms;
		}

		[delegate.manager stopUpdatingLocation];

		if (delegate.location) {
			CLLocationCoordinate2D c = delegate.location.coordinate;

			std::ostringstream os;
			os << "{"
			   << "\"ok\":true,"
			   << "\"provider\":\"corelocation\","
			   << "\"latitude\":" << c.latitude << ","
			   << "\"longitude\":" << c.longitude << ","
			   << "\"accuracyMeters\":" << delegate.location.horizontalAccuracy << ","
			   << "\"altitudeMeters\":" << delegate.location.altitude
			   << "}";
			return os.str();
		}

		if (delegate.lastError) {
			return make_error_json([[delegate.lastError localizedDescription] UTF8String]);
		}

		return make_error_json("Timed out waiting for macOS location");
	}
}
#endif
#endif

static std::string detect_location_linux()
{
	const char *cmd =
		"sh -lc '"
		"command -v gdbus >/dev/null 2>&1 || { echo \"{\\\"ok\\\":false,\\\"error\\\":\\\"gdbus not found\\\"}\"; exit 0; }; "
		"CLIENT=$(gdbus call --system "
		"--dest org.freedesktop.GeoClue2 "
		"--object-path /org/freedesktop/GeoClue2/Manager "
		"--method org.freedesktop.GeoClue2.Manager.GetClient 2>/dev/null | "
		"sed -E \"s/.*'([^']+)'.*/\\1/\"); "
		"[ -n \"$CLIENT\" ] || { echo \"{\\\"ok\\\":false,\\\"error\\\":\\\"GeoClue client creation failed\\\"}\"; exit 0; }; "
		"gdbus call --system --dest org.freedesktop.GeoClue2 --object-path \"$CLIENT\" "
		"--method org.freedesktop.DBus.Properties.Set "
		"org.freedesktop.GeoClue2.Client DesktopId \"<'com.taffylabs.obs.plugin'>\" >/dev/null 2>&1; "
		"gdbus call --system --dest org.freedesktop.GeoClue2 --object-path \"$CLIENT\" "
		"--method org.freedesktop.DBus.Properties.Set "
		"org.freedesktop.GeoClue2.Client RequestedAccuracyLevel '<uint32 8>' >/dev/null 2>&1; "
		"gdbus call --system --dest org.freedesktop.GeoClue2 --object-path \"$CLIENT\" "
		"--method org.freedesktop.GeoClue2.Client.Start >/dev/null 2>&1; "
		"sleep 2; "
		"LOC=$(gdbus call --system "
		"--dest org.freedesktop.GeoClue2 "
		"--object-path \"$CLIENT\" "
		"--method org.freedesktop.DBus.Properties.Get "
		"org.freedesktop.GeoClue2.Client Location 2>/dev/null | sed -E \"s/.*objectpath '([^']+)'.*/\\1/\"); "
		"[ -n \"$LOC\" ] || { echo \"{\\\"ok\\\":false,\\\"error\\\":\\\"No GeoClue location available\\\"}\"; exit 0; }; "
		"LAT=$(gdbus call --system --dest org.freedesktop.GeoClue2 --object-path \"$LOC\" "
		"--method org.freedesktop.DBus.Properties.Get org.freedesktop.GeoClue2.Location Latitude 2>/dev/null | sed -E \"s/.*double ([^)]*).*/\\1/\"); "
		"LON=$(gdbus call --system --dest org.freedesktop.GeoClue2 --object-path \"$LOC\" "
		"--method org.freedesktop.DBus.Properties.Get org.freedesktop.GeoClue2.Location Longitude 2>/dev/null | sed -E \"s/.*double ([^)]*).*/\\1/\"); "
		"ACC=$(gdbus call --system --dest org.freedesktop.GeoClue2 --object-path \"$LOC\" "
		"--method org.freedesktop.DBus.Properties.Get org.freedesktop.GeoClue2.Location Accuracy 2>/dev/null | sed -E \"s/.*double ([^)]*).*/\\1/\"); "
		"echo \"{\\\"ok\\\":true,\\\"provider\\\":\\\"geoclue\\\",\\\"latitude\\\":$LAT,\\\"longitude\\\":$LON,\\\"accuracyMeters\\\":$ACC}\""
		"'";

	std::string out = exec_capture(cmd);
	if (out.empty())
		return make_error_json("Linux geolocation command failed");
	return out;
}

static std::string get_windows_uuid()
{
	return trim(exec_capture(
		"powershell -NoProfile -ExecutionPolicy Bypass -Command "
		"\"(Get-CimInstance -ClassName Win32_ComputerSystemProduct).UUID\""));
}

static std::string get_mac_uuid()
{
	return trim(exec_capture(
		"sh -lc \"ioreg -rd1 -c IOPlatformExpertDevice | awk -F\\\" '/IOPlatformUUID/{print $(NF-1)}'\""));
}

static std::string get_linux_machine_id()
{
	std::string id;
	if (read_whole_file("/etc/machine-id", id))
		return trim(id);
	if (read_whole_file("/var/lib/dbus/machine-id", id))
		return trim(id);
	return {};
}

static void append_json_number(std::ostringstream &os, const char *name, double value, bool &first)
{
	if (!first)
		os << ",";
	first = false;
	os << "\"" << name << "\":" << value;
}

static void append_json_int(std::ostringstream &os, const char *name, long long value, bool &first)
{
	if (!first)
		os << ",";
	first = false;
	os << "\"" << name << "\":" << value;
}

static void append_json_bool(std::ostringstream &os, const char *name, bool value, bool &first)
{
	if (!first)
		os << ",";
	first = false;
	os << "\"" << name << "\":" << (value ? "true" : "false");
}

static void append_json_string(std::ostringstream &os, const char *name, const std::string &value, bool &first)
{
	if (!first)
		os << ",";
	first = false;
	os << "\"" << name << "\":\"" << json_escape(value) << "\"";
}

struct SceneEnumContext {
	std::ostringstream *os;
	bool firstItem;
};

static void write_scene_items_recursive(std::ostringstream &os, obs_scene_t *scene);

static bool enum_scene_item_callback(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	(void)scene;

	SceneEnumContext *ctx = (SceneEnumContext *)param;
	if (!ctx || !ctx->os)
		return false;

	std::ostringstream &os = *ctx->os;

	if (!ctx->firstItem)
		os << ",";
	ctx->firstItem = false;

	obs_source_t *src = obs_sceneitem_get_source(item);
	const char *source_name = src ? obs_source_get_name(src) : "";
	const char *source_id = src ? obs_source_get_id(src) : "";

	vec2 pos = {};
	vec2 scale = {};
	vec2 bounds = {};
	obs_sceneitem_crop crop = {};

	obs_sceneitem_get_pos(item, &pos);
	obs_sceneitem_get_scale(item, &scale);
	obs_sceneitem_get_bounds(item, &bounds);
	obs_sceneitem_get_crop(item, &crop);

	float rot = obs_sceneitem_get_rot(item);
	uint32_t alignment = obs_sceneitem_get_alignment(item);
	uint32_t bounds_alignment = obs_sceneitem_get_bounds_alignment(item);
	int bounds_type = (int)obs_sceneitem_get_bounds_type(item);
	bool visible = obs_sceneitem_visible(item);
	bool locked = obs_sceneitem_locked(item);
	bool is_group = obs_sceneitem_is_group(item);
	long long item_id = (long long)obs_sceneitem_get_id(item);
	long long order_pos = (long long)obs_sceneitem_get_order_position(item);

	os << "{";
	bool first = true;

	append_json_int(os, "itemId", item_id, first);
	append_json_int(os, "orderPosition", order_pos, first);
	append_json_string(os, "sourceName", source_name ? source_name : "", first);
	append_json_string(os, "sourceId", source_id ? source_id : "", first);
	append_json_bool(os, "visible", visible, first);
	append_json_bool(os, "locked", locked, first);
	append_json_bool(os, "isGroup", is_group, first);
	append_json_number(os, "x", pos.x, first);
	append_json_number(os, "y", pos.y, first);
	append_json_number(os, "rotationDegrees", rot, first);
	append_json_number(os, "scaleX", scale.x, first);
	append_json_number(os, "scaleY", scale.y, first);
	append_json_int(os, "alignment", alignment, first);
	append_json_int(os, "boundsType", bounds_type, first);
	append_json_int(os, "boundsAlignment", bounds_alignment, first);
	append_json_number(os, "boundsWidth", bounds.x, first);
	append_json_number(os, "boundsHeight", bounds.y, first);

	if (!first)
		os << ",";
	os << "\"crop\":{"
	   << "\"left\":" << crop.left << ","
	   << "\"top\":" << crop.top << ","
	   << "\"right\":" << crop.right << ","
	   << "\"bottom\":" << crop.bottom
	   << "}";

	if (is_group) {
		obs_scene_t *group_scene = obs_sceneitem_group_get_scene(item);
		os << ",\"children\":[";
		if (group_scene)
			write_scene_items_recursive(os, group_scene);
		os << "]";
	}

	os << "}";
	return true;
}

static void write_scene_items_recursive(std::ostringstream &os, obs_scene_t *scene)
{
	SceneEnumContext ctx;
	ctx.os = &os;
	ctx.firstItem = true;
	obs_scene_enum_items(scene, enum_scene_item_callback, &ctx);
}

static std::string get_best_location_json_cpp(int timeout_ms)
{
#ifdef _WIN32
	(void)timeout_ms;
	return detect_location_windows();
#elif defined(__APPLE__)
	#ifdef __OBJC__
	return detect_location_mac(timeout_ms);
	#else
	(void)timeout_ms;
	return make_error_json("On macOS, compile this file as Objective-C++ (.mm) to use CoreLocation");
	#endif
#elif defined(__linux__)
	(void)timeout_ms;
	return detect_location_linux();
#else
	(void)timeout_ms;
	return make_error_json("Unsupported platform");
#endif
}

static std::string get_unique_device_id_json_cpp()
{
#ifdef _WIN32
	std::string uuid = get_windows_uuid();
	if (uuid.empty())
		return make_error_json("Could not read Win32_ComputerSystemProduct.UUID");
	return make_device_id_json("windows", "Win32_ComputerSystemProduct.UUID", uuid);
#elif defined(__APPLE__)
	std::string uuid = get_mac_uuid();
	if (uuid.empty())
		return make_error_json("Could not read IOPlatformUUID");
	return make_device_id_json("macos", "IOPlatformUUID", uuid);
#elif defined(__linux__)
	std::string id = get_linux_machine_id();
	if (id.empty())
		return make_error_json("Could not read /etc/machine-id");
	return make_device_id_json("linux", "machine-id", id);
#else
	return make_error_json("Unsupported platform");
#endif
}

static std::string get_obs_current_program_scene_json_cpp()
{
	obs_source_t *scene_source = obs_frontend_get_current_scene();
	if (!scene_source)
		return make_error_json("OBS returned no current scene");

	const char *scene_name = obs_source_get_name(scene_source);
	obs_scene_t *scene = obs_scene_from_source(scene_source);

	if (!scene) {
		obs_source_release(scene_source);
		return make_error_json("Current source is not a scene");
	}

	std::ostringstream os;
	os << "{"
	   << "\"ok\":true,"
	   << "\"sceneName\":\"" << json_escape(scene_name ? scene_name : "") << "\","
	   << "\"items\":[";

	write_scene_items_recursive(os, scene);

	os << "]}";

	obs_source_release(scene_source);
	return os.str();
}

} // namespace

extern "C" char *taffylabs_get_best_location_json(int timeout_ms)
{
	return dup_cstr(get_best_location_json_cpp(timeout_ms));
}

extern "C" char *taffylabs_get_unique_device_id_json(void)
{
	return dup_cstr(get_unique_device_id_json_cpp());
}

extern "C" char *taffylabs_get_obs_current_program_scene_json(void)
{
	return dup_cstr(get_obs_current_program_scene_json_cpp());
}

extern "C" void taffylabs_device_info_free_string(char *str)
{
	free(str);
}