#include <obs-module.h>
#include <obs-frontend-api.h>
#include "chalk-mode.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-chalk", "en-US")

extern struct obs_source_info chalk_source_info;

static void on_obs_event(obs_frontend_event event, void *)
{
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
        chalk_mode_install();
    else if (event == OBS_FRONTEND_EVENT_EXIT)
        chalk_mode_shutdown();
}

bool obs_module_load(void)
{
    obs_register_source(&chalk_source_info);
    obs_frontend_add_event_callback(on_obs_event, nullptr);
    return true;
}

void obs_module_unload(void)
{
    obs_frontend_remove_event_callback(on_obs_event, nullptr);
}
