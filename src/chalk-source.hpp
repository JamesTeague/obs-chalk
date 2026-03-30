#pragma once
#include <obs-module.h>
#include "tool-state.hpp"
#include "mark-list.hpp"

struct ChalkSource {
    obs_source_t *source;
    ToolState tool_state;
    MarkList mark_list;
    bool drawing = false;  // true while left mouse button is held

    // Hotkey registration IDs
    obs_hotkey_id hotkey_undo             = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_clear            = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_color            = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_laser            = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_tool_freehand    = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_tool_arrow       = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_tool_circle      = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_tool_cone        = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_pick_delete      = OBS_INVALID_HOTKEY_ID;

    // Laser pointer state
    bool  laser_active = false;
    float laser_x      = 0.f;
    float laser_y      = 0.f;

    // Pick-to-delete mode
    bool pick_delete_mode = false;

    explicit ChalkSource(obs_source_t *src) : source(src) {}
};
