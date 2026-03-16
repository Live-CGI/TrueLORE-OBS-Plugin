#include "lore-api.h"
#include "taffy_json_http.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

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
			result.ok = 0;
			result.error_text = dup_cstr("Missing JSON payload");
			return result;
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