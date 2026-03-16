#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

static obs_output_t  *g_mirror_output  = NULL;
static obs_service_t *g_mirror_service = NULL;

static const char *DEFAULT_SERVER = "rtmp://oh.elevensports.live/Live";
static const char *MIRROR_KEY    = "TAFFY";

/* If you really want to keep exactly one constant string:
   rtmp_target="rtmp://oh.elevensports.live/Live/TAFFY"
   then parse it into server/key. For now: hardcode server+key like above.
*/

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
	if(STREAM_SERVER == NULL){
		STREAM_SERVER = DEFAULT_SERVER;
	}
	obs_data_set_string(svc_settings, "server", STREAM_SERVER);
	obs_data_set_string(svc_settings, "key", STREAM_KEY);

	/* "rtmp_custom" is OBS’s built-in Custom RTMP service */
	g_mirror_service = obs_service_create("rtmp_custom", "taffylabs_mirror_service",
	                                     svc_settings, NULL);

	obs_data_release(svc_settings);
}

bool taffylabs_mirror_start(const char* STREAM_SERVER, const char* STREAM_KEY)
{
    blog(LOG_INFO, "[TaffyLabs] starting mirror...");
    
	if (g_mirror_output) {
		blog(LOG_WARNING, "[TaffyLabs] mirror already started?");
		return true;
	}

	obs_output_t *primary = obs_frontend_get_streaming_output();
	if (!primary) {
		blog(LOG_WARNING, "[TaffyLabs] no primary streaming output available");
		return false;
	}

	/* Create service + output */
	mirror_build_service(STREAM_SERVER, STREAM_KEY);
	if (!g_mirror_service) {
		blog(LOG_ERROR, "[TaffyLabs] failed to create mirror RTMP service");
		return false;
	}

	g_mirror_output = obs_output_create("rtmp_output", "taffylabs_mirror_output", NULL, NULL);

	if (!g_mirror_output) {
		blog(LOG_ERROR, "[TaffyLabs] failed to create mirror RTMP output");
		mirror_release_all();
		return false;
	}

	obs_output_set_service(g_mirror_output, g_mirror_service);

	/* Share encoders from the primary stream output (NO re-encode) */
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

	blog(LOG_INFO, "[TaffyLabs] mirror started -> %s (key %s)", STREAM_SERVER, STREAM_KEY);
	return true;
}

void taffylabs_mirror_stop(void)
{
    blog(LOG_INFO, "[TaffyLabs] stop mirror requested...");
	if (!g_mirror_output){
        blog(LOG_WARNING, "[TaffyLabs] mirror not running, take a nap instead.");
        return;
    }
	blog(LOG_INFO, "[TaffyLabs] stopping mirror");
	obs_output_stop(g_mirror_output);

	/* For baby steps: release immediately.
	   Later: you can wait for a “stopped” signal if you want to be extra clean. */
	mirror_release_all();
}