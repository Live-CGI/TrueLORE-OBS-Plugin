#include "lore-api.h"
#include "taffy_json_http.h"
#include "lore-deviceInfo.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <random>
#include <utility>
#include <time.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
	static const char *kGenericUrl = "https://httpbin.org/post";
	static const std::string kLoreApiUrl = "https://dev-api.tome-lore.com/";
	
	static char *dup_cstr(const std::string &s)
	{
		char *out = (char *)std::malloc(s.size() + 1);
		if (!out)
			return nullptr;

		std::memcpy(out, s.c_str(), s.size() + 1);
		return out;
	}

    static lore_api_result build_result(const taffy_json_http_response &resp)
    {
        lore_api_result out{};
        out.status_code = resp.status_code;
        out.ok = (resp.status_code >= 200 && resp.status_code < 300) ? 1 : 0;

        if (resp.body) {
            out.body_text = dup_cstr(resp.body);

            if (!json::accept(resp.body)) {
                out.error_text = dup_cstr("JSON parse failed: invalid JSON");
            }
        }

        return out;
    }

	struct AsyncContext {
		lore_api_callback callback = nullptr;
		void *user_data = nullptr;

		std::string method;
		std::string url;
		std::string body;
		std::vector<std::string> header_storage;
		std::vector<const char *> headers;
	};

	static void async_bridge(const taffy_json_http_response *resp, void *user_data)
	{
		std::unique_ptr<AsyncContext> ctx(static_cast<AsyncContext *>(user_data));
		if (!ctx || !ctx->callback || !resp)
			return;

		lore_api_result result = build_result(*resp);
		ctx->callback(&result, ctx->user_data);
		lore_api_free_result(&result);
	}
}

char *taffylabs_create_metadata_json(void)
{
    time_t now = time(NULL);
    struct tm utc;

#if defined(_WIN32)
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif

    char date[11];     // YYYY-MM-DD
    char timeStr[9];   // HH.mm.ss

    strftime(date, sizeof(date), "%Y-%m-%d", &utc);
    strftime(timeStr, sizeof(timeStr), "%H.%M.%S", &utc);

    const char *deviceJson = taffylabs_get_unique_device_id_json();

    if (!deviceJson)
        deviceJson = "null";

    char title[128];
    char description[256];

    snprintf(title, sizeof(title),
        "New Broadcast %s %s",
        date,
        timeStr);

    snprintf(description, sizeof(description),
        "New Broadcast created at %s UTC on %s",
        timeStr,
        date);

    const char *format =
        "{"
        "\"metadata\":{"
        "\"title\":\"%s\","
        "\"description\":\"%s\","
        "\"deviceInfo\":%s"
        "}"
        "}";

    size_t needed = snprintf(NULL, 0, format, title, description, deviceJson) + 1;

    char *json = (char *)malloc(needed);
    if (!json)
        return NULL;

    snprintf(json, needed, format, title, description, deviceJson);

    return json;
}

extern "C" const char* random_hex_32()
{
    static std::string out;
    static const char *hex = "0123456789abcdef";

    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 255);

    out.resize(64);

    for (int i = 0; i < 32; ++i) {
        unsigned char b = dist(rd);
        out[i*2]     = hex[b >> 4];
        out[i*2 + 1] = hex[b & 0x0F];
    }

    return out.c_str();
}

extern "C" lore_api_result lore_authDeviceBegin(
	const char* hexID, 
	lore_api_callback callback)
{
	std::string strHexId = std::string(hexID);
	std::string entryPointTmp = kLoreApiUrl + "auth/device?rid=" + strHexId;
	const char* entryPoint = entryPointTmp.c_str();
	if (!callback) {
		taffy_json_http_response resp =
			taffy_json_http_request("GET", entryPoint, nullptr, nullptr, 0);

		lore_api_result result = build_result(resp);
		taffy_json_http_free(&resp);
		return result;
	}

	AsyncContext *ctx = new AsyncContext();
	ctx->callback = callback;
	//ctx->user_data = user_data;

	const int started = taffy_json_http_request_async(
		"GET",
		entryPoint,
		nullptr,
		nullptr,
		0,
		async_bridge,
		ctx);

	if (!started) {
		delete ctx;

		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("Failed to start async HTTP request");
		return result;
	}

	lore_api_result result{};
	result.ok = 1;
	return result;
}

extern "C" lore_api_result lore_authDeviceContinue(
	const char* hexID, 
	const char* stateID, 
	lore_api_callback callback)
{
	std::string strHexId = std::string(hexID);
	std::string strState = std::string(stateID);
	std::string entryPointTemp = kLoreApiUrl + "auth/device?rid=" + strHexId + "&state=" + strState;
	const char* entryPoint = entryPointTemp.c_str();
	
	if (!callback) {
		taffy_json_http_response resp =
			taffy_json_http_request("GET", entryPoint, nullptr, nullptr, 0);

		lore_api_result result = build_result(resp);
		taffy_json_http_free(&resp);
		return result;
	}

	AsyncContext *ctx = new AsyncContext();
	ctx->callback = callback;
	//ctx->user_data = user_data;

	const int started = taffy_json_http_request_async(
		"GET",
		entryPoint,
		nullptr,
		nullptr,
		0,
		async_bridge,
		ctx);

	if (!started) {
		delete ctx;

		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("Failed to start async HTTP request");
		return result;
	}

	lore_api_result result{};
	result.ok = 1;
	return result;
}

extern "C" lore_api_result lore_get(lore_api_callback callback)
{
	const char* entryPoint = (kLoreApiUrl).c_str();
	if (!callback) {
		taffy_json_http_response resp =
			taffy_json_http_request("GET", entryPoint, nullptr, nullptr, 0);

		lore_api_result result = build_result(resp);
		taffy_json_http_free(&resp);
		return result;
	}

	AsyncContext *ctx = new AsyncContext();
	ctx->callback = callback;
	//ctx->user_data = user_data;

	const int started = taffy_json_http_request_async(
		"GET",
		entryPoint,
		nullptr,
		nullptr,
		0,
		async_bridge,
		ctx);

	if (!started) {
		delete ctx;

		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("Failed to start async HTTP request");
		return result;
	}

	lore_api_result result{};
	result.ok = 1;
	return result;
}

extern "C" lore_api_result lore_keys(const char *bearer, lore_api_callback callback)
{
	std::string authHeader = std::string("Authorization: Bearer ") + bearer;
	const char* headers[] = {
		authHeader.c_str(),
	};

	const char* entryPoint = (kLoreApiUrl + "keys").c_str();
	if (!callback) {

		taffy_json_http_response resp =
			taffy_json_http_request("GET", entryPoint, nullptr, headers, 1);

		lore_api_result result = build_result(resp);
		taffy_json_http_free(&resp);
		return result;
	}

	AsyncContext *ctx = new AsyncContext();
	ctx->callback = callback;
	//ctx->user_data = user_data;

	const int started = taffy_json_http_request_async(
		"GET",
		entryPoint,
		nullptr,
		headers,
		1,
		async_bridge,
		ctx);

	if (!started) {
		delete ctx;

		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("Failed to start async HTTP request");
		return result;
	}

	lore_api_result result{};
	result.ok = 1;
	return result;
}

extern "C" lore_api_result lore_keys_generate(const char *bearer,lore_api_callback callback)
{
	std::string authHeader = std::string("Authorization: Bearer ") + bearer;
	const char* headers[] = {
		authHeader.c_str(),
	};
	const char* entryPoint = (kLoreApiUrl + "keys/generate").c_str();
	if (!callback) {

		taffy_json_http_response resp =
			taffy_json_http_request("GET", entryPoint, nullptr, headers, 1);

		lore_api_result result = build_result(resp);
		taffy_json_http_free(&resp);
		return result;
	}

	AsyncContext *ctx = new AsyncContext();
	ctx->callback = callback;
	//ctx->user_data = user_data;

	const int started = taffy_json_http_request_async(
		"GET",
		entryPoint,
		nullptr,
		headers,
		1,
		async_bridge,
		ctx);

	if (!started) {
		delete ctx;

		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("Failed to start async HTTP request");
		return result;
	}

	lore_api_result result{};
	result.ok = 1;
	return result;
}

extern "C" lore_api_result lore_keys_getPublic(const char *bearer,const char *keyId, lore_api_callback callback)
{
	std::string authHeader = std::string("Authorization: Bearer ") + bearer;
	const char* headers[] = {
		authHeader.c_str(),
	};
	std::string sEntryPoint = kLoreApiUrl + "keys/" + keyId + "/get";   
	const char* entryPoint = sEntryPoint.c_str();
	if (!callback) {

		taffy_json_http_response resp =
			taffy_json_http_request("GET", entryPoint, nullptr, headers, 1);

		lore_api_result result = build_result(resp);
		taffy_json_http_free(&resp);
		return result;
	}

	AsyncContext *ctx = new AsyncContext();
	ctx->callback = callback;
	//ctx->user_data = user_data;

	const int started = taffy_json_http_request_async(
		"GET",
		entryPoint,
		nullptr,
		headers,
		1,
		async_bridge,
		ctx);

	if (!started) {
		delete ctx;

		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("Failed to start async HTTP request");
		return result;
	}

	lore_api_result result{};
	result.ok = 1;
	return result;
}

extern "C" lore_api_result lore_keys_deletePublic(const char *bearer,const char *keyId, lore_api_callback callback)
{
	std::string authHeader = std::string("Authorization: Bearer ") + bearer;
	const char* headers[] = {
		authHeader.c_str(),
	};
	std::string sEntryPoint = kLoreApiUrl + "keys/" + keyId;   
	const char* entryPoint = sEntryPoint.c_str();
	if (!callback) {

		taffy_json_http_response resp =
			taffy_json_http_request("DELETE", entryPoint, nullptr, headers, 1);

		lore_api_result result = build_result(resp);
		taffy_json_http_free(&resp);
		return result;
	}

	AsyncContext *ctx = new AsyncContext();
	ctx->callback = callback;
	//ctx->user_data = user_data;

	const int started = taffy_json_http_request_async(
		"DELETE",
		entryPoint,
		nullptr,
		headers,
		1,
		async_bridge,
		ctx);

	if (!started) {
		delete ctx;

		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("Failed to start async HTTP request");
		return result;
	}

	lore_api_result result{};
	result.ok = 1;
	return result;
}

	extern "C" lore_api_result lore_broadcastsCreate(
		const char *bearer,
		const char *jsonData,
		lore_api_callback callback,
		void *user_data)
	{
		lore_api_result result{};
		
		if (!bearer || !*bearer) {
			result.ok = 0;
			result.error_text = dup_cstr("Missing bearer token");
			return result;
		}
		
		if (!jsonData) {
			//result.ok = 0;
			//result.error_text = dup_cstr("Missing JSON payload");
			//return result;
			jsonData = taffylabs_create_metadata_json();

		}

		const std::string authHeader =
			std::string("Authorization: Bearer ") + bearer;
		const std::string entryPoint = kLoreApiUrl + "broadcasts";

		// Synchronous request
		if (!callback) {
			const char *headers[] = {
				authHeader.c_str(),
			};

			taffy_json_http_response resp = taffy_json_http_request(
				"POST",
				entryPoint.c_str(),
				jsonData,
				headers,
				1);

			result = build_result(resp);
			taffy_json_http_free(&resp);
			return result;
		}

		// Asynchronous request
		AsyncContext *ctx = new AsyncContext();
		ctx->callback = callback;
		ctx->user_data = user_data;
		ctx->method = "POST";
		ctx->url = entryPoint;
		ctx->body = jsonData;
		ctx->header_storage.push_back(authHeader);

		// Build stable char* header array backed by header_storage
		for (const std::string &h : ctx->header_storage)
			ctx->headers.push_back(h.c_str());

		const int started = taffy_json_http_request_async(
			ctx->method.c_str(),
			ctx->url.c_str(),
			ctx->body.c_str(),
			ctx->headers.empty() ? nullptr : ctx->headers.data(),
			static_cast<int>(ctx->headers.size()),
			async_bridge,
			ctx);

		if (!started) {
			delete ctx;

			result.ok = 0;
			result.error_text = dup_cstr("Failed to start async HTTP request");
			return result;
		}

		result.ok = 1;
		return result;
	}

extern "C" lore_api_result lore_broadcastsGet(
	const char *bearer,
	const char *keyId,
	lore_api_callback callback,
	void *user_data)
{
	std::string authHeader = std::string("Authorization: Bearer ") +
		(bearer ? bearer : "");

	const char *headers[] = {
		authHeader.c_str(),
	};

	std::string sEntryPoint = kLoreApiUrl + "broadcasts";
	if (keyId && *keyId) {
		sEntryPoint += "/";
		sEntryPoint += keyId;
	}

	const char *entryPoint = sEntryPoint.c_str();

	if (!callback) {
		taffy_json_http_response resp =
			taffy_json_http_request("GET", entryPoint, nullptr, headers, 1);

		lore_api_result result = build_result(resp);
		taffy_json_http_free(&resp);
		return result;
	}

	AsyncContext *ctx = new AsyncContext();
	ctx->callback = callback;
	ctx->user_data = user_data;

	const int started = taffy_json_http_request_async(
		"GET",
		entryPoint,
		nullptr,
		headers,
		1,
		async_bridge,
		ctx);

	if (!started) {
		delete ctx;

		lore_api_result result{};
		result.ok = 0;
		result.status_code = 0;
		result.body_text = nullptr;
		result.error_text = dup_cstr("Failed to start async HTTP request");
		return result;
	}

	lore_api_result result{};
	result.ok = 1;
	result.status_code = 0;
	result.body_text = nullptr;
	result.error_text = nullptr;
	return result;
}

extern "C" lore_api_result lore_broadcastsUpdate(const char *bearer, const char *keyId, const char *jsonData,  lore_api_callback callback)
{
	std::string authHeader = std::string("Authorization: Bearer ") + bearer;
	const char *headers[] = {
		authHeader.c_str(),
	};
	std::string sEntryPoint = kLoreApiUrl + "broadcasts";   
	if(keyId){
		sEntryPoint += "/" + std::string(keyId);
	}else{
		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("keyId is required for this endpoint!");
		return result;
	}

	if(jsonData == nullptr){
		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("jsonData is required for this endpoint!");
		return result;
	}

	const char* entryPoint = sEntryPoint.c_str();
	if (!callback) {

		taffy_json_http_response resp =
			taffy_json_http_request("PATCH", entryPoint, jsonData, headers, 1);

		lore_api_result result = build_result(resp);
		taffy_json_http_free(&resp);
		return result;
	}

	AsyncContext *ctx = new AsyncContext();
	ctx->callback = callback;
	//ctx->user_data = user_data;

	const int started = taffy_json_http_request_async(
		"PATCH",
		entryPoint,
		jsonData,
		headers,
		1,
		async_bridge,
		ctx);

	if (!started) {
		delete ctx;

		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("Failed to start async HTTP request");
		return result;
	}

	lore_api_result result{};
	result.ok = 1;
	return result;
}

extern "C" lore_api_result lore_broadcastsDelete(const char *bearer, const char *keyId, lore_api_callback callback, void *user_data)
{
	std::string authHeader = std::string("Authorization: Bearer ") + bearer;
	const char* headers[] = {
		authHeader.c_str(),
	};

	std::string sEntryPoint = kLoreApiUrl + "broadcasts";   
	if(keyId){
		sEntryPoint += "/" + std::string(keyId);
	}else{
		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("keyId is required for this endpoint!");
		return result;
	}
	const char* entryPoint = sEntryPoint.c_str();
	if (!callback) {

		taffy_json_http_response resp =
			taffy_json_http_request("DELETE", entryPoint, nullptr, headers, 1);

		lore_api_result result = build_result(resp);
		taffy_json_http_free(&resp);
		return result;
	}

	AsyncContext *ctx = new AsyncContext();
	ctx->callback = callback;
	//ctx->user_data = user_data;

	const int started = taffy_json_http_request_async(
		"DELETE",
		entryPoint,
		nullptr,
		headers,
		1,
		async_bridge,
		ctx);

	if (!started) {
		delete ctx;

		lore_api_result result{};
		result.ok = 0;
		result.error_text = dup_cstr("Failed to start async HTTP request");
		return result;
	}

	lore_api_result result{};
	result.ok = 1;
	return result;
}

extern "C" void lore_api_free_result(lore_api_result *result)
{
	if (!result)
		return;

	if (result->body_text) {
		std::free(result->body_text);
		result->body_text = nullptr;
	}

	if (result->error_text) {
		std::free(result->error_text);
		result->error_text = nullptr;
	}

	result->status_code = 0;
	result->ok = 0;
}