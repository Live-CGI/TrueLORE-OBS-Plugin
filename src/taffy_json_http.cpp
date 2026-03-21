#include "taffy_json_http.h"
#include <curl/curl.h>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static std::once_flag g_curl_init_once;

static void ensure_curl_global_init()
{
    std::call_once(g_curl_init_once, []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}

struct Buffer {
    char *data = nullptr;
    size_t size = 0;
    size_t cap = 0;
};

static bool buffer_reserve(Buffer &b, size_t needed)
{
    if (needed <= b.cap) return true;
    size_t new_cap = b.cap ? b.cap : 4096;
    while (new_cap < needed) new_cap *= 2;

    char *p = (char *)std::realloc(b.data, new_cap);
    if (!p) return false;
    b.data = p;
    b.cap = new_cap;
    return true;
}

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    const size_t n = size * nmemb;
    Buffer *buf = (Buffer *)userdata;

    if (!buffer_reserve(*buf, buf->size + n + 1))
        return 0;

    std::memcpy(buf->data + buf->size, ptr, n);
    buf->size += n;
    buf->data[buf->size] = '\0';
    return n;
}

static curl_slist *build_headers(
    const char *json_body,
    const char *const *extra_headers,
    size_t extra_header_count)
{
    curl_slist *hdrs = nullptr;

    hdrs = curl_slist_append(hdrs, "Accept: application/json");

    if (json_body && json_body[0] != '\0') {
        hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    }

    for (size_t i = 0; i < extra_header_count; ++i) {
        const char *h = extra_headers ? extra_headers[i] : nullptr;
        if (h && h[0] != '\0') {
            hdrs = curl_slist_append(hdrs, h);
        }
    }

    return hdrs;
}

static taffy_json_http_response perform(
    const char *method,
    const char *url,
    const char *json_body,
    const char *const *extra_headers,
    size_t extra_header_count)
{
    taffy_json_http_response out{};
    Buffer buf{};

    ensure_curl_global_init();

    CURL *curl = curl_easy_init();
    if (!curl) {
        return out;
    }

    curl_slist *hdrs = build_headers(json_body, extra_headers, extra_header_count);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    const char *m = method ? method : "GET";

    if (std::strcmp(m, "GET") == 0) {
        // default
    } else if (std::strcmp(m, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body ? json_body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)(json_body ? std::strlen(json_body) : 0));
    } else if (std::strcmp(m, "PUT") == 0 || std::strcmp(m, "PATCH") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, m);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body ? json_body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)(json_body ? std::strlen(json_body) : 0));
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, m);
        if (json_body && json_body[0] != '\0') {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)std::strlen(json_body));
        }
    }

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out.status_code);
        out.body = buf.data;
        buf.data = nullptr;
    } else {
        out.status_code = 0;
    }

    if (buf.data) std::free(buf.data);
    if (hdrs) curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    return out;
}

extern "C" taffy_json_http_response taffy_json_http_request(
    const char *method,
    const char *url,
    const char *json_body,
    const char *const *headers,
    size_t header_count)
{
    if (!url) return {};
    return perform(method, url, json_body, headers, header_count);
}

extern "C" int taffy_json_http_request_async(
    const char *method,
    const char *url,
    const char *json_body,
    const char *const *headers,
    size_t header_count,
    taffy_json_http_async_cb callback,
    void *user_data)
{
    if (!url || !callback) return 0;

    std::string m = method ? method : "GET";
    std::string u = url;
    std::string b = json_body ? json_body : "";

    std::vector<std::string> header_storage;
    header_storage.reserve(header_count);

    for (size_t i = 0; i < header_count; ++i) {
        const char *h = headers ? headers[i] : nullptr;
        if (h) {
            header_storage.emplace_back(h);
        }
    }

    std::thread(
        [m = std::move(m),
         u = std::move(u),
         b = std::move(b),
         header_storage = std::move(header_storage),
         callback,
         user_data]() mutable
        {
            std::vector<const char *> header_ptrs;
            header_ptrs.reserve(header_storage.size());

            for (const std::string &s : header_storage) {
                header_ptrs.push_back(s.c_str());
            }

            taffy_json_http_response resp = perform(
                m.c_str(),
                u.c_str(),
                b.empty() ? nullptr : b.c_str(),
                header_ptrs.empty() ? nullptr : header_ptrs.data(),
                header_ptrs.size());

            callback(&resp, user_data);

            if (resp.body) std::free(resp.body);
        }).detach();

    return 1;
}

extern "C" void taffy_json_http_free(taffy_json_http_response *resp)
{
    if (!resp) return;
    if (resp->body) {
        std::free(resp->body);
        resp->body = nullptr;
    }
    resp->status_code = 0;
}
