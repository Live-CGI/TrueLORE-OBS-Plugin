#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define TAFFY_SECTION                  "taffylabs"
#define KEY_ENABLED                    "enabled"
#define KEY_OPTIONAL_METADATA_ENABLED  "optional_metadata_enabled"
#define KEY_TITLE                      "title"
#define KEY_DESCRIPTION                "description"
#define KEY_BEARER_TOKEN               "bearerToken"
#define KEY_STREAMKEY                  "streamKey"
#define KEY_BROADCAST                  "broadCast"

#define TAFFY_TEXT_MAX 1024

static bool g_enabled = true;
static bool g_optional_metadata_enabled = false;
static char g_title[TAFFY_TEXT_MAX] = "";
static char g_description[TAFFY_TEXT_MAX] = "";
static char g_bearer_token[TAFFY_TEXT_MAX] = "";
static char g_streamKey[TAFFY_TEXT_MAX] = "";
static char g_selectedBroadcastObject[TAFFY_TEXT_MAX] = "";
static char g_BroadcastArray[1024*1024*5] = "";

static void copy_text(char *dst, size_t dst_size, const char *src)
{
	if (!dst || dst_size == 0)
		return;

	if (!src)
		src = "";

	snprintf(dst, dst_size, "%s", src);
}

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

const char *taffylabs_get_streamKey(void)
{
	return g_streamKey;
}

void taffylabs_set_streamKey(const char *streamKey)
{
	copy_text(g_streamKey, sizeof(g_streamKey), streamKey);
}

const char *taffylabs_get_bearer_token(void)
{
	return g_bearer_token;
}

void taffylabs_set_bearer_token(const char *token)
{
	copy_text(g_bearer_token, sizeof(g_bearer_token), token);
}

const char *taffylabs_get_selected_broadcast_object(void)
{
	return g_selectedBroadcastObject;
}
void taffylabs_set_selected_broadcast_object(const char *broadcastObject)
{
	copy_text(g_selectedBroadcastObject, sizeof(g_selectedBroadcastObject),
		  broadcastObject);
}
	
const char *taffylabs_get_broadcast_array(void){
	return g_BroadcastArray;
}

void taffylabs_set_broadcast_array(const char *broadcastArray)
{
	copy_text(g_BroadcastArray, sizeof(g_BroadcastArray),broadcastArray);
}

void taffylabs_save_settings(void)
{
	config_t *cfg = obs_frontend_get_user_config();
	if (!cfg)
		return;

	config_set_bool(cfg, TAFFY_SECTION, KEY_ENABLED, g_enabled);
	config_set_bool(cfg, TAFFY_SECTION, KEY_OPTIONAL_METADATA_ENABLED,
			g_optional_metadata_enabled);
	config_set_string(cfg, TAFFY_SECTION, KEY_TITLE, g_title);
	config_set_string(cfg, TAFFY_SECTION, KEY_DESCRIPTION, g_description);
	config_set_string(cfg, TAFFY_SECTION, KEY_STREAMKEY, g_streamKey);
	config_set_string(cfg, TAFFY_SECTION, KEY_BROADCAST, g_selectedBroadcastObject);
	config_set_string(cfg, TAFFY_SECTION, KEY_BEARER_TOKEN, g_bearer_token);
	config_save(cfg);
	
}

void taffylabs_load_settings(void)
{
	config_t *cfg = obs_frontend_get_user_config();

	// Defaults first
	g_enabled = true;
	g_optional_metadata_enabled = false;

	copy_text(g_title, sizeof(g_title), "");
	copy_text(g_description, sizeof(g_description), "");
	copy_text(g_streamKey, sizeof(g_streamKey), "");
	copy_text(g_selectedBroadcastObject, sizeof(g_selectedBroadcastObject), "");
	copy_text(g_BroadcastArray, sizeof(g_BroadcastArray), "");
	copy_text(g_bearer_token, sizeof(g_bearer_token), "");

	if (!cfg)
		return;

	g_enabled = config_get_bool(cfg, TAFFY_SECTION, KEY_ENABLED);
	g_optional_metadata_enabled =
		config_get_bool(cfg, TAFFY_SECTION, KEY_OPTIONAL_METADATA_ENABLED);

	copy_text(g_title, sizeof(g_title),
		  config_get_string(cfg, TAFFY_SECTION, KEY_TITLE));

	copy_text(g_description, sizeof(g_description),
		  config_get_string(cfg, TAFFY_SECTION, KEY_DESCRIPTION));

	copy_text(g_streamKey, sizeof(g_streamKey),
		  config_get_string(cfg, TAFFY_SECTION, KEY_STREAMKEY));

	copy_text(g_selectedBroadcastObject,
		  sizeof(g_selectedBroadcastObject),
		  config_get_string(cfg, TAFFY_SECTION, KEY_BROADCAST));

	copy_text(g_bearer_token, sizeof(g_bearer_token),
		  config_get_string(cfg, TAFFY_SECTION, KEY_BEARER_TOKEN));
}