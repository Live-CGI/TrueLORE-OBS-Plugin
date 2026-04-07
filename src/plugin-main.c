/*
Plugin Name
Copyright (C) <Year> <Developer> <Email Address>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <string.h>

#include "obs_stream_keyframe_enforcer.h"
#include "taffylabs_ui.h"
#include "taffylabs_settings.h"
#include "taffylabs_mirror.h"
#include "taffy_json_http.h"
#include "lore-api.h"
#include "lore-deviceInfo.h"


OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static void on_frontend_event(enum obs_frontend_event event, void *private_data)
{
	if (!taffylabs_get_enabled()){
		return;
	}
	
	UNUSED_PARAMETER(private_data);

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		blog(LOG_INFO, "[TaffyLabs] streaming starting...");
		if(taffylabs_get_enforce_2k_fps()){
			blog(LOG_INFO, "[TaffyLabs] Enforcing 2 Keyframes per second minimum...");
			taffylabs_enforce_stream_keyframe_interval();
		}
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STARTED: {
		taffylabs_mirror_start();
		break;
	}


	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		blog(LOG_INFO, "[TaffyLabs] streaming stopping...");
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		blog(LOG_INFO, "[TaffyLabs] streaming stopped!");
		taffylabs_mirror_stop();
		break;

	default:
		break;
	}
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	//Get Them Settings!
	taffylabs_load_settings();
	const char *resp = taffylabs_get_unique_device_id_json();
	if (resp && strstr(resp, "{\"ok\":true")) {
		taffylabs_set_unique_id(resp);
		taffylabs_save_settings();
	} else {
		blog(LOG_WARNING, "[TaffyLabs] Could not get unique device ID: %s", resp ? resp : "no response");
	}
	taffylabs_ui_init();
	obs_frontend_add_event_callback(on_frontend_event, NULL);
	blog(LOG_INFO, "[TaffyLabs] loaded (enabled=%s)",taffylabs_get_enabled() ? "true" : "false");
	return true;
}

void obs_module_unload(void)
{
	taffylabs_ui_shutdown();
	obs_log(LOG_INFO, "plugin unloaded");
}

// end of file

