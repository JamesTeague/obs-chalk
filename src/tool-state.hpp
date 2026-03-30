#pragma once
#include <cstdint>

enum class ToolType : uint8_t {
    Freehand = 0,
    Arrow,
    Circle,
    Cone,
    Laser,
};

static constexpr uint32_t CHALK_PALETTE[] = {
    0xFFFFFFFF,  // white
    0xFF00FFFF,  // yellow (R=FF, G=FF, B=00 in ABGR)
    0xFF0000FF,  // red
    0xFFFF0000,  // blue
    0xFF00FF00,  // green
};
static constexpr int CHALK_PALETTE_SIZE = 5;

struct ToolState {
    ToolType active_tool  = ToolType::Freehand;
    int      color_index  = 0;

    uint32_t active_color() const { return CHALK_PALETTE[color_index]; }
};
