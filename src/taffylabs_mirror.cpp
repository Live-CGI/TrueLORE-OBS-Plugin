#include "taffylabs_mirror.h"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <nlohmann/json.hpp>

#include "taffylabs_settings.h"
#include "lore-api.h"
#include "taffylabs_ui.h"

static obs_output_t  *g_mirror_output  = NULL;
static obs_service_t *g_mirror_service = NULL;



static void mirror_release_all(void)
{
	if (g_mirror_output) {
		obs_output_release(g_mirror_output);
		g_mirror_output = NULL;
	}
	if (g_mirror_service) {
		obs_service_release(g_mirror_service);
		g_mirror_service = NULL;
	}
}

static void mirror_build_service(const char* STREAM_SERVER, const char* STREAM_KEY)
{
	obs_data_t *svc_settings = obs_data_create();
	if (STREAM_SERVER == NULL || STREAM_SERVER[0] == '\0') {
		STREAM_SERVER = taffylabs_get_streamServer();
	}
	obs_data_set_string(svc_settings, "server", STREAM_SERVER);
	obs_data_set_string(svc_settings, "key", STREAM_KEY);

	g_mirror_service = obs_service_create(
		"rtmp_custom",
		"taffylabs_mirror_service",
		svc_settings,
		NULL);

	obs_data_release(svc_settings);
}

extern "C" {

bool taffylabs_mirror_start(void)
{
	blog(LOG_INFO, "[TrueLORE] autoBroadcast enabled, creating new broadcast before starting mirror...");

	if (g_mirror_output) {
		blog(LOG_WARNING, "[TrueLORE] mirror already started?");
		return true;
	}

	lore_api_result rNew = lore_broadcastsCreate(
		taffylabs_get_bearer_token(), NULL, NULL, NULL);

	if (!rNew.ok) {
		blog(LOG_WARNING, "[TrueLORE] failed to create new broadcast: %s", rNew.error_text ? rNew.error_text : "unknown error");
		taffylabs_update_status_indicator("Failed to create new broadcast!");
		taffylabs_show_error("Failed to create new broadcast!", rNew.error_text ? rNew.error_text : "unknown error");	
		return false;
	} else {
		blog(LOG_INFO, "[TrueLORE] created new broadcast successfully!");
	}

	taffylabs_set_selected_broadcast_object(rNew.body_text);
	obs_output_t *primary = obs_frontend_get_streaming_output();
	if (!primary) {
		blog(LOG_WARNING, "[TrueLORE] no primary streaming output available");
		taffylabs_update_status_indicator("No primary streaming output available!");
		taffylabs_show_error("No primary streaming output available!", "Please start streaming in OBS first.");
		return false;
	}

	const char *STREAM_SERVER = taffylabs_get_streamServer();
	const char *STREAM_KEY = taffylabs_get_streamKey();

	mirror_build_service(STREAM_SERVER, STREAM_KEY);
	if (!g_mirror_service) {
		blog(LOG_ERROR, "[TrueLORE] failed to create mirror RTMP service");
		taffylabs_update_status_indicator("Failed to create mirror RTMP service!");
		taffylabs_show_error("Failed to create mirror RTMP service!", "Please check your stream server and stream key.");
		return false;
	}else{
		blog(LOG_INFO, "[TrueLORE] mirror RTMP service created successfully\n" 
			    "    Server    = %s\n" 
				"    Stream Key= %s\n", 
				STREAM_SERVER, STREAM_KEY);
	}

	g_mirror_output = obs_output_create("rtmp_output", "taffylabs_mirror_output", NULL, NULL);
	if (!g_mirror_output) {
		blog(LOG_ERROR, "[TrueLORE] failed to create mirror RTMP output");
		taffylabs_update_status_indicator("Failed to create mirror RTMP output!");
		taffylabs_show_error("Failed to create mirror RTMP output!", "Please check your stream server and stream key.");
		mirror_release_all();
		return false;
	}

	obs_output_set_service(g_mirror_output, g_mirror_service);

	obs_encoder_t *venc = obs_output_get_video_encoder(primary);
	obs_encoder_t *aenc = obs_output_get_audio_encoder(primary, 0);

	if (!venc || !aenc) {
		blog(LOG_ERROR, "[TrueLORE] failed to get encoders from primary output");
		taffylabs_update_status_indicator("Failed to get encoders from primary output!");
		mirror_release_all();
		taffylabs_show_error("Failed to get encoders from primary output!", "Please check your stream server and stream key.");
		return false;
	}

	obs_output_set_video_encoder(g_mirror_output, venc);
	obs_output_set_audio_encoder(g_mirror_output, aenc, 0);

	if (!obs_output_start(g_mirror_output)) {
		blog(LOG_ERROR, "[TrueLORE] mirror obs_output_start failed");
		taffylabs_update_status_indicator("Failed to start mirror!");
		mirror_release_all();
		taffylabs_show_error("Failed to start mirror!", "Please check your stream server and stream key.");
		return false;
	}
	blog(LOG_INFO, "[TrueLORE] mirror started successfully!");

	std::string msg = "Streaming to: " + std::string(taffylabs_get_title());
	taffylabs_update_status_indicator(msg.c_str());
	return true;
}

void taffylabs_mirror_stop(void)
{
	blog(LOG_INFO, "[TrueLORE] stop mirror requested...");

	if (!g_mirror_output) {
		blog(LOG_WARNING, "[TrueLORE] mirror not running, take a nap instead.");
		taffylabs_update_status_indicator("Mirror not running!");
		return;
	}

	blog(LOG_INFO, "[TrueLORE] stopping mirror");
	obs_output_stop(g_mirror_output);
	mirror_release_all();
	taffylabs_update_status_indicator("Mirror stopped successfully!");
}

} // extern "C"

