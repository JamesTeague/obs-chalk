#pragma once
#include <obs-module.h>
#include "tool-state.hpp"
#include "mark-list.hpp"

struct ChalkSource {
    obs_source_t *source;
    ToolState tool_state;
    MarkList mark_list;
    bool drawing = false;  // true while left mouse button is held

    explicit ChalkSource(obs_source_t *src) : source(src) {}
};
