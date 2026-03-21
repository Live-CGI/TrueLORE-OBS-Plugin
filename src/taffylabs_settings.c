#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "aes_string.h"

#define TAFFY_SECTION                  "taffylabs"
#define KEY_ENABLED                    "enabled"
#define KEY_AUTO_BROADCAST             "auto_broadcast"
#define KEY_OPTIONAL_METADATA_ENABLED  "optional_metadata_enabled"
#define KEY_TITLE                      "title"
#define KEY_DESCRIPTION                "description"
#define KEY_BEARER_TOKEN               "bearerToken"
#define KEY_STREAMKEY                  "streamKey"
#define KEY_STREAMSERVER               "streamServer"
#define KEY_BROADCAST                  "broadCast"
#define KEY_UNIQUE_ID                  "uniqueID"
#define TAFFY_TEXT_MAX 2048

static bool g_enabled = true;
static bool g_optional_metadata_enabled = false;
static bool g_autoBroadcast = true;
static char g_title[TAFFY_TEXT_MAX] = "";
static char g_description[TAFFY_TEXT_MAX] = "";
static char g_bearer_token[TAFFY_TEXT_MAX] = "";
static char g_streamKey[TAFFY_TEXT_MAX] = "";
static char g_streamServer[TAFFY_TEXT_MAX] = "";
static char g_selectedBroadcastObject[TAFFY_TEXT_MAX] = "";
static char g_BroadcastArray[1024*1024*5] = "";
static char g_UniqueID[TAFFY_TEXT_MAX] = "";

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

bool taffylabs_get_autoBroadcast(void)
{
	return g_autoBroadcast;
}

void taffylabs_set_autoBroadcast(bool enabled)
{
	g_autoBroadcast = enabled;
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
	const char* decrypted = taffy_aes_decrypt_string(g_bearer_token);
	blog(LOG_INFO, "[TaffyLabs] Getting bearer token, decrypted value is '%s'", decrypted ? decrypted : "null");
	return decrypted;
}

void taffylabs_set_bearer_token(const char *token)
{
	blog(LOG_INFO, "[TaffyLabs] Setting bearer token to '%s'", token ? token : "null");
	const char* encrypted = taffy_aes_encrypt_string(token);
	copy_text(g_bearer_token, sizeof(g_bearer_token), encrypted);
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

const char *taffylabs_get_unique_id(void)
{
	return g_UniqueID;
}

void taffylabs_set_unique_id(const char *uniqueID)
{
	copy_text(g_UniqueID, sizeof(g_UniqueID), uniqueID);
}

const char *taffylabs_get_streamServer(void)
{
	return g_streamServer;
}

void taffylabs_set_streamServer(const char *streamServer)
{
	copy_text(g_streamServer, sizeof(g_streamServer), streamServer);
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
	config_set_string(cfg, TAFFY_SECTION, KEY_UNIQUE_ID, g_UniqueID);
	config_set_string(cfg, TAFFY_SECTION, KEY_STREAMSERVER, g_streamServer);
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
	copy_text(g_UniqueID, sizeof(g_UniqueID), "");
	copy_text(g_streamServer, sizeof(g_streamServer), "");
	copy_text(g_selectedBroadcastObject, sizeof(g_selectedBroadcastObject), "");

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

	copy_text(g_streamServer, sizeof(g_streamServer),
		  config_get_string(cfg, TAFFY_SECTION, KEY_STREAMSERVER));

	copy_text(g_selectedBroadcastObject,
		  sizeof(g_selectedBroadcastObject),
		  config_get_string(cfg, TAFFY_SECTION, KEY_BROADCAST));

	copy_text(g_bearer_token, sizeof(g_bearer_token),
		  config_get_string(cfg, TAFFY_SECTION, KEY_BEARER_TOKEN));

	copy_text(g_UniqueID, sizeof(g_UniqueID),
		  config_get_string(cfg, TAFFY_SECTION, KEY_UNIQUE_ID));
}

void taffylabs_clear_settings(void)
{
	config_t *cfg = obs_frontend_get_user_config();

	// Defaults first
	g_enabled = false;
	g_optional_metadata_enabled = false;
	copy_text(g_title, sizeof(g_title), "");
	copy_text(g_description, sizeof(g_description), "");
	copy_text(g_streamKey, sizeof(g_streamKey), "");
	copy_text(g_selectedBroadcastObject, sizeof(g_selectedBroadcastObject), "");
	copy_text(g_BroadcastArray, sizeof(g_BroadcastArray), "");
	copy_text(g_bearer_token, sizeof(g_bearer_token), "");
	
	if (!cfg)
		return;

	taffylabs_save_settings();
}

// end of file
