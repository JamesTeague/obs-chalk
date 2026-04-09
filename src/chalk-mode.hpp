#pragma once
// Lifecycle: called from plugin.cpp frontend event callback
void chalk_mode_install();   // Called on OBS_FRONTEND_EVENT_FINISHED_LOADING
void chalk_mode_shutdown();  // Called on OBS_FRONTEND_EVENT_EXIT

// Returns true when chalk mode (draw overlay) is currently active.
// Called from ChalkDock::refresh() on Qt main thread.
bool chalk_mode_is_active();
