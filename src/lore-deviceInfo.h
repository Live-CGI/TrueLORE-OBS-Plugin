#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * All returned strings are UTF-8 JSON and must be freed with
 * taffylabs_device_info_free_string().
 *
 * Example:
 *   char *json = taffylabs_get_best_location_json(10000);
 *   if (json) {
 *       blog(LOG_INFO, "%s", json);
 *       taffylabs_device_info_free_string(json);
 *   }
 */

/* Best-effort device geolocation as JSON */
char *taffylabs_get_best_location_json(int timeout_ms);

/* Stable-ish device identifier as JSON */
char *taffylabs_get_unique_device_id_json(void);

/* Current OBS Program scene as JSON */
char *taffylabs_get_obs_current_program_scene_json(void);

/* Free strings returned by this API */
void taffylabs_device_info_free_string(char *str);

#ifdef __cplusplus
}
#endif