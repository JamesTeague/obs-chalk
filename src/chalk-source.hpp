#pragma once
#include <obs-module.h>
#include "tool-state.hpp"
#include "mark-list.hpp"

// ---------------------------------------------------------------------------
// Shared input dispatch — called by both source callbacks and event filter
// ---------------------------------------------------------------------------

void chalk_input_begin(struct ChalkSource *ctx, float x, float y, float pressure = 1.0f);
void chalk_input_move(struct ChalkSource *ctx, float x, float y, float pressure = 1.0f);
void chalk_input_end(struct ChalkSource *ctx);

// Returns the first ChalkSource in the current scene, or nullptr
struct ChalkSource *chalk_find_source();

// ---------------------------------------------------------------------------

struct ChalkSource {
    obs_source_t *source;
    ToolState tool_state;
    MarkList mark_list;
    bool drawing = false;  // true while left mouse button is held

    // Hotkey registration IDs
    obs_hotkey_id hotkey_undo             = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_clear            = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_color            = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_tool_freehand    = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_tool_arrow       = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_tool_circle      = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_tool_cone        = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_tool_laser       = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id hotkey_tool_delete      = OBS_INVALID_HOTKEY_ID;

    // Laser pointer state
    bool  laser_active = false;
    float laser_x      = 0.f;
    float laser_y      = 0.f;

    // Scene-transition settings
    bool clear_on_scene_change = false;

    explicit ChalkSource(obs_source_t *src) : source(src) {}
};
