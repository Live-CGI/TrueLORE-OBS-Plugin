#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct taffy_json_http_response {
    long status_code;
    char *body;
} taffy_json_http_response;

typedef void (*taffy_json_http_async_cb)(
    const taffy_json_http_response *resp,
    void *user_data);

taffy_json_http_response taffy_json_http_request(
    const char *method,
    const char *url,
    const char *json_body,
    const char *const *headers,
    size_t header_count);

int taffy_json_http_request_async(
    const char *method,
    const char *url,
    const char *json_body,
    const char *const *headers,
    size_t header_count,
    taffy_json_http_async_cb callback,
    void *user_data);

void taffy_json_http_free(taffy_json_http_response *resp);

#ifdef __cplusplus
}
#endif