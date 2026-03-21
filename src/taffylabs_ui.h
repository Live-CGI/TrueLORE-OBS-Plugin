#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void taffylabs_ui_init(void);
void taffylabs_ui_shutdown(void);

void taffylabs_show_message(const char *title, const char *message);
void taffylabs_show_warning(const char *title, const char *message);
void taffylabs_show_error(const char *title, const char *message);

#ifdef __cplusplus
}
#endif
