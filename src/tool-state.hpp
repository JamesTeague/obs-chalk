#pragma once
#include <cstdint>

enum class ToolType : uint8_t {
    Freehand = 0,
    // Arrow, Circle, Cone, Laser added in Phase 2
};

struct ToolState {
    ToolType active_tool = ToolType::Freehand;
    uint32_t active_color = 0xFFFFFFFF; // ABGR white
};
