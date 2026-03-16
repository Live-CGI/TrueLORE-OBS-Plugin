#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool taffylabs_get_enabled(void);
void taffylabs_set_enabled(bool enabled);

bool taffylabs_get_optional_metadata_enabled(void);
void taffylabs_set_optional_metadata_enabled(bool enabled);

const char *taffylabs_get_title(void);
void taffylabs_set_title(const char *title);

const char *taffylabs_get_description(void);
void taffylabs_set_description(const char *description);

const char *taffylabs_get_bearer_token(void);
void taffylabs_set_bearer_token(const char *token);

const char *taffylabs_get_streamKey(void);
void taffylabs_set_streamKey(const char *streamKey);

const char *taffylabs_get_selected_broadcast_object(void);
void taffylabs_set_selected_broadcast_object(const char *streamKey);

const char *taffylabs_get_broadcast_array(void);
void taffylabs_set_broadcast_array(const char *streamKey);

void taffylabs_load_settings(void);
void taffylabs_save_settings(void);

#ifdef __cplusplus
}
#endif