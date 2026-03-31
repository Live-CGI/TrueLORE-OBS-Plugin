#include "taffylabs_mirror.h"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <nlohmann/json.hpp>

#include "taffylabs_settings.h"
#include "lore-api.h"

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
	blog(LOG_INFO, "[TaffyLabs] autoBroadcast enabled, creating new broadcast before starting mirror...");

	if (g_mirror_output) {
		blog(LOG_WARNING, "[TaffyLabs] mirror already started?");
		return true;
	}

	lore_api_result rNew = lore_broadcastsCreate(
		taffylabs_get_bearer_token(), NULL, NULL, NULL);

	if (!rNew.ok) {
		blog(LOG_WARNING, "[TaffyLabs] failed to create new broadcast: %s",
		     rNew.error_text ? rNew.error_text : "unknown error");
		return false;
	} else {
		blog(LOG_INFO, "[TaffyLabs] created new broadcast successfully!");
	}

	taffylabs_set_selected_broadcast_object(rNew.body_text);


	obs_output_t *primary = obs_frontend_get_streaming_output();
	if (!primary) {
		blog(LOG_WARNING, "[TaffyLabs] no primary streaming output available");
		return false;
	}

	const char *STREAM_SERVER = taffylabs_get_streamServer();
	const char *STREAM_KEY = taffylabs_get_streamKey();

	mirror_build_service(STREAM_SERVER, STREAM_KEY);
	if (!g_mirror_service) {
		blog(LOG_ERROR, "[TaffyLabs] failed to create mirror RTMP service");
		return false;
	}else{
		blog(LOG_INFO, "[TaffyLabs] mirror RTMP service created successfully\n" 
			    "    Server    = %s\n" 
				"    Stream Key= %s\n", 
				STREAM_SERVER, STREAM_KEY);
	}

	g_mirror_output = obs_output_create("rtmp_output", "taffylabs_mirror_output", NULL, NULL);
	if (!g_mirror_output) {
		blog(LOG_ERROR, "[TaffyLabs] failed to create mirror RTMP output");
		mirror_release_all();
		return false;
	}

	obs_output_set_service(g_mirror_output, g_mirror_service);

	obs_encoder_t *venc = obs_output_get_video_encoder(primary);
	obs_encoder_t *aenc = obs_output_get_audio_encoder(primary, 0);

	if (!venc || !aenc) {
		blog(LOG_ERROR, "[TaffyLabs] failed to get encoders from primary output");
		mirror_release_all();
		return false;
	}

	obs_output_set_video_encoder(g_mirror_output, venc);
	obs_output_set_audio_encoder(g_mirror_output, aenc, 0);

	if (!obs_output_start(g_mirror_output)) {
		blog(LOG_ERROR, "[TaffyLabs] mirror obs_output_start failed");
		mirror_release_all();
		return false;
	}
	blog(LOG_INFO, "[TaffyLabs] mirror started successfully!");

	//Set the State completed! ***NOT NECESSARY? HANDLED BY BACKEND?***
	//const char* BCAST_ID = taffylabs_get_streamID();
	//nlohmann::json jsonSetLive = {
	//	{"state", "live"}
	//};
	//lore_api_result rUpdate = lore_broadcastsUpdate(
	//	taffylabs_get_bearer_token(),
	//	BCAST_ID,
	//	jsonSetLive.dump().c_str(),
	//	NULL);
	
	//if(rUpdate.ok!=1){
	//	blog(LOG_ERROR, "[TaffyLabs] failed to set broadcast state to live: %s", rUpdate.error_text ? rUpdate.error_text : "unknown error");
	//}else{
	//	blog(LOG_INFO, "[TaffyLabs] broadcast state set to live successfully!");
	//}
	return true;
}

void taffylabs_mirror_stop(void)
{
	blog(LOG_INFO, "[TaffyLabs] stop mirror requested...");

	if (!g_mirror_output) {
		blog(LOG_WARNING, "[TaffyLabs] mirror not running, take a nap instead.");
		return;
	}

	blog(LOG_INFO, "[TaffyLabs] stopping mirror");
	obs_output_stop(g_mirror_output);
	mirror_release_all();
	//Set the State completed! ***NOT NECESSARY? HANDLED BY BACKEND?***
	//const char* BCAST_ID = taffylabs_get_streamID();
	//nlohmann::json jsonSetLive = {
	//	{"state", "complete"}
	//};
	//lore_api_result rUpdate = lore_broadcastsUpdate(
	//	taffylabs_get_bearer_token(),
	//	BCAST_ID,
	//	jsonSetLive.dump().c_str(),
	//	NULL);
	
	//if(rUpdate.ok!=1){
	//	blog(LOG_ERROR, "[TaffyLabs] failed to set broadcast state to completed: %s", rUpdate.error_text ? rUpdate.error_text : "unknown error");
	//}else{
    //	blog(LOG_INFO, "[TaffyLabs] broadcast state set to completed successfully!");	
	//}
}

} // extern "C"