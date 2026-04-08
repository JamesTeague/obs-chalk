#pragma once
// Lifecycle: called from plugin.cpp frontend event callback
void chalk_mode_install();   // Called on OBS_FRONTEND_EVENT_FINISHED_LOADING
void chalk_mode_shutdown();  // Called on OBS_FRONTEND_EVENT_EXIT
