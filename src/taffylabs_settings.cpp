#include "taffylabs_settings.h"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <cstdio>
#include <cstring>
#include <string>

// Optional: now available since this is C++
#include <nlohmann/json.hpp>

#include "crypto_string.h"

#define TAFFY_SECTION                  "taffylabs"
#define KEY_ENABLED                    "enabled"
#define KEY_AUTO_BROADCAST             "auto_broadcast"
#define KEY_OPTIONAL_METADATA_ENABLED  "optional_metadata_enabled"
#define KEY_ENFORCE_2K_FPS             "enforce_2k_fps"
#define KEY_TITLE                      "title"
#define KEY_DESCRIPTION                "description"
#define KEY_BEARER_TOKEN               "bearerToken"
#define KEY_BROADCAST                  "broadCast"
#define KEY_UNIQUE_ID                  "uniqueID"
#define TAFFY_TEXT_MAX                 2048
#define TAFFY_BROADCAST_ARRAY_MAX      (1024 * 1024 * 5)

namespace
{
	static const char *DEFAULT_SERVER = "rtmps://dev-rtmp.tome-lore.com:443/live";

	static bool g_enabled = true;
	static bool g_optional_metadata_enabled = false;
	static bool g_autoBroadcast = true;
	static bool g_enforce2KFPS = false;

	static char g_title[TAFFY_TEXT_MAX] = "";
	static char g_description[TAFFY_TEXT_MAX] = "";
	static char g_bearer_token[TAFFY_TEXT_MAX] = "";
	static char g_streamKey[TAFFY_TEXT_MAX] = "";
	static char g_streamServer[TAFFY_TEXT_MAX] = "";
	static char g_streamID[TAFFY_TEXT_MAX] = "";
	static char g_selectedBroadcastObject[TAFFY_TEXT_MAX] = "";
	static char g_BroadcastArray[TAFFY_BROADCAST_ARRAY_MAX] = "";
	static char g_UniqueID[TAFFY_TEXT_MAX] = "";

	static void copy_text(char *dst, size_t dst_size, const char *src)
	{
		if (!dst || dst_size == 0)
			return;

		if (!src)
			src = "";

		std::snprintf(dst, dst_size, "%s", src);
	}

	static bool is_valid_json(const char *text)
	{
		if (!text || !*text)
			return false;

		return nlohmann::json::accept(text);
	}

	static bool get_selected_broadcast_json(nlohmann::json &jsonObj)
	{
		if (!g_selectedBroadcastObject[0])
			return false;

		jsonObj = nlohmann::json::parse(g_selectedBroadcastObject, nullptr, false);
		return !jsonObj.is_discarded();
	}
}

extern "C" {

bool taffylabs_get_enabled(void)
{
	return g_enabled;
}

void taffylabs_set_enabled(bool enabled)
{
	g_enabled = enabled;
}

bool taffylabs_get_optional_metadata_enabled(void)
{
	return g_optional_metadata_enabled;
}

void taffylabs_set_optional_metadata_enabled(bool enabled)
{
	g_optional_metadata_enabled = enabled;
}

bool taffylabs_get_enforce_2k_fps(void)
{
	return g_enforce2KFPS;
}

void taffylabs_set_enforce_2k_fps(bool enforce)
{
	g_enforce2KFPS = enforce;
}

const char *taffylabs_get_title(void)
{
	return g_title;
}

void taffylabs_set_title(const char *title)
{
	copy_text(g_title, sizeof(g_title), title);
}

const char *taffylabs_get_description(void)
{
	return g_description;
}

void taffylabs_set_description(const char *description)
{
	copy_text(g_description, sizeof(g_description), description);
}

const char *taffylabs_get_streamServer(void)
{
	nlohmann::json jsonObj;
	if (get_selected_broadcast_json(jsonObj)) {
		std::string streamServerFromJson =
			jsonObj.value("stream_server", std::string(DEFAULT_SERVER));
		copy_text(g_streamServer, sizeof(g_streamServer), streamServerFromJson.c_str());
	} else {
		copy_text(g_streamServer, sizeof(g_streamServer), DEFAULT_SERVER);
	}

	return g_streamServer;
}

const char *taffylabs_get_streamKey(void)
{
	nlohmann::json jsonObj;
	if (get_selected_broadcast_json(jsonObj)) {
		std::string streamKeyFromJson =
			jsonObj.value("stream_key", std::string(""));
		copy_text(g_streamKey, sizeof(g_streamKey), streamKeyFromJson.c_str());
	} else {
		copy_text(g_streamKey, sizeof(g_streamKey), "");
	}

	return g_streamKey;
}

const char *taffylabs_get_streamID(void)
{
	nlohmann::json jsonObj;
	if (get_selected_broadcast_json(jsonObj)) {
		std::string streamIDFromJson =
			jsonObj.value("id", std::string(""));
		copy_text(g_streamID, sizeof(g_streamID), streamIDFromJson.c_str());
	} else {
		copy_text(g_streamID, sizeof(g_streamID), "");
	}

	return g_streamID;
}

const char *taffylabs_get_bearer_token(void)
{
	const char *decrypted = taffylabs_decrypt_string(g_bearer_token);
	return decrypted;
}

void taffylabs_set_bearer_token(const char *token)
{
	const char *encrypted = taffylabs_encrypt_string(token);
	copy_text(g_bearer_token, sizeof(g_bearer_token), encrypted);
}

const char *taffylabs_get_selected_broadcast_object(void)
{
	return g_selectedBroadcastObject;
}

void taffylabs_set_selected_broadcast_object(const char *broadcastObject)
{
	if (is_valid_json(broadcastObject)) {
		copy_text(g_selectedBroadcastObject, sizeof(g_selectedBroadcastObject), broadcastObject);
		blog(LOG_INFO, "[TaffyLabs] Selected broadcast object set successfully. ID=%s",
		     taffylabs_get_streamID());
	} else {
		blog(LOG_ERROR,
		     "[TaffyLabs] Invalid JSON for selected broadcast object, ignoring.\n Data: %s",
		     broadcastObject ? broadcastObject : "(null)");
	}
}

const char *taffylabs_get_broadcast_array(void)
{
	return g_BroadcastArray;
}

void taffylabs_set_broadcast_array(const char *broadcastArray)
{
	copy_text(g_BroadcastArray, sizeof(g_BroadcastArray), broadcastArray);
}

const char *taffylabs_get_unique_id(void)
{
	return g_UniqueID;
}

void taffylabs_set_unique_id(const char *uniqueID)
{
	copy_text(g_UniqueID, sizeof(g_UniqueID), uniqueID);
}

void taffylabs_set_streamServer(const char *streamServer)
{
	copy_text(g_streamServer, sizeof(g_streamServer), streamServer);
}

void taffylabs_save_settings(void)
{
	blog(LOG_INFO, "[TaffyLabs] Saving settings...");

	config_t *cfg = obs_frontend_get_user_config();
	if (!cfg)
		return;

	config_set_bool(cfg, TAFFY_SECTION, KEY_ENABLED, g_enabled);
	config_set_bool(cfg, TAFFY_SECTION, KEY_OPTIONAL_METADATA_ENABLED, g_optional_metadata_enabled);
	config_set_bool(cfg, TAFFY_SECTION, KEY_ENFORCE_2K_FPS, g_enforce2KFPS);
	config_set_string(cfg, TAFFY_SECTION, KEY_TITLE, g_title);
	config_set_string(cfg, TAFFY_SECTION, KEY_DESCRIPTION, g_description);
	config_set_string(cfg, TAFFY_SECTION, KEY_BROADCAST, g_selectedBroadcastObject);
	config_set_string(cfg, TAFFY_SECTION, KEY_BEARER_TOKEN, g_bearer_token);
	config_set_string(cfg, TAFFY_SECTION, KEY_UNIQUE_ID, g_UniqueID);
	config_save(cfg);
}

void taffylabs_load_settings(void)
{
	blog(LOG_INFO, "[TaffyLabs] Loading settings...");

	config_t *cfg = obs_frontend_get_user_config();

	g_enabled = true;
	g_optional_metadata_enabled = false;
	g_autoBroadcast = true;
	g_enforce2KFPS = false;

	copy_text(g_title, sizeof(g_title), "");
	copy_text(g_description, sizeof(g_description), "");
	copy_text(g_streamKey, sizeof(g_streamKey), "");
	copy_text(g_streamServer, sizeof(g_streamServer), "");
	copy_text(g_streamID, sizeof(g_streamID), "");
	copy_text(g_selectedBroadcastObject, sizeof(g_selectedBroadcastObject), "");
	copy_text(g_BroadcastArray, sizeof(g_BroadcastArray), "");
	copy_text(g_bearer_token, sizeof(g_bearer_token), "");
	copy_text(g_UniqueID, sizeof(g_UniqueID), "");

	if (!cfg)
		return;

	g_enabled = config_get_bool(cfg, TAFFY_SECTION, KEY_ENABLED);
	g_optional_metadata_enabled = config_get_bool(cfg, TAFFY_SECTION, KEY_OPTIONAL_METADATA_ENABLED);
	g_enforce2KFPS = config_get_bool(cfg, TAFFY_SECTION, KEY_ENFORCE_2K_FPS);

	copy_text(g_title, sizeof(g_title),
		config_get_string(cfg, TAFFY_SECTION, KEY_TITLE));

	copy_text(g_description, sizeof(g_description),
		config_get_string(cfg, TAFFY_SECTION, KEY_DESCRIPTION));

	copy_text(g_selectedBroadcastObject, sizeof(g_selectedBroadcastObject),
		config_get_string(cfg, TAFFY_SECTION, KEY_BROADCAST));

	copy_text(g_bearer_token, sizeof(g_bearer_token),
		config_get_string(cfg, TAFFY_SECTION, KEY_BEARER_TOKEN));

	copy_text(g_UniqueID, sizeof(g_UniqueID),
		config_get_string(cfg, TAFFY_SECTION, KEY_UNIQUE_ID));
}

void taffylabs_clear_settings(void)
{
	blog(LOG_INFO, "[TaffyLabs] Clearing settings...");

	g_enabled = false;
	g_optional_metadata_enabled = false;
	g_autoBroadcast = true;
	g_enforce2KFPS = false;

	copy_text(g_title, sizeof(g_title), "");
	copy_text(g_description, sizeof(g_description), "");
	copy_text(g_streamKey, sizeof(g_streamKey), "");
	copy_text(g_streamServer, sizeof(g_streamServer), "");
	copy_text(g_streamID, sizeof(g_streamID), "");
	copy_text(g_selectedBroadcastObject, sizeof(g_selectedBroadcastObject), "");
	copy_text(g_BroadcastArray, sizeof(g_BroadcastArray), "");
	copy_text(g_bearer_token, sizeof(g_bearer_token), "");
	copy_text(g_UniqueID, sizeof(g_UniqueID), "");

	taffylabs_save_settings();
}

} // extern "C"
