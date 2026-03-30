#pragma once
#include <obs-module.h>
#include "tool-state.hpp"

struct ChalkSource {
    obs_source_t *source;
    ToolState tool_state;

    explicit ChalkSource(obs_source_t *src) : source(src) {}
};
