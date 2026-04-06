#include "obs_stream_keyframe_enforcer.h"

extern "C" {
#include <obs-module.h>
#include <obs-frontend-api.h>
}

static bool enforce_stream_keyframe_interval_impl()
{
    obs_output_t *output = obs_frontend_get_streaming_output();
    if (!output) {
        blog(LOG_WARNING, "[TaffyLabs] No streaming output available during STREAMING_STARTING");
        return false;
    }

    obs_encoder_t *video_encoder = obs_output_get_video_encoder(output);
    if (!video_encoder) {
        blog(LOG_WARNING, "[TaffyLabs] No video encoder on current streaming output");
        obs_output_release(output);
        return false;
    }

    obs_data_t *settings = obs_encoder_get_settings(video_encoder);
    if (!settings) {
        blog(LOG_WARNING, "[TaffyLabs] Failed to get streaming encoder settings");
        obs_output_release(output);
        return false;
    }

    long long keyint_sec = obs_data_get_int(settings, "keyint_sec");
    const char *encoder_name = obs_encoder_get_name(video_encoder);
    const char *encoder_id = obs_encoder_get_id(video_encoder);

    blog(LOG_INFO,
         "[TaffyLabs] Streaming encoder check: name='%s' id='%s' keyint_sec=%lld",
         encoder_name ? encoder_name : "(unknown)",
         encoder_id ? encoder_id : "(unknown)",
         keyint_sec);

    bool changed = false;

    if (keyint_sec == 0 || keyint_sec > 2) {
        obs_data_set_int(settings, "keyint_sec", 2);
        obs_encoder_update(video_encoder, settings);
        changed = true;

        blog(LOG_WARNING,
             "[TaffyLabs] Enforced runtime keyframe interval to 2 seconds "
             "(previous keyint_sec=%lld)",
             keyint_sec);
    }

    obs_data_release(settings);
    obs_output_release(output);
    return changed;
}

extern "C" bool taffylabs_enforce_stream_keyframe_interval(void)
{
    return enforce_stream_keyframe_interval_impl();
}