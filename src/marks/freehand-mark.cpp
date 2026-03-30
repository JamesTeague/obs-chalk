#include "freehand-mark.hpp"
#include <graphics/graphics.h>

FreehandMark::FreehandMark(float r, float g, float b, float a)
{
    vec4_set(&color_, r, g, b, a);
}

void FreehandMark::add_point(float x, float y)
{
    points_.push_back({x, y});
}

void FreehandMark::draw(gs_eparam_t *color_param) const
{
    if (points_.size() < 2)
        return;

    gs_effect_set_vec4(color_param, &color_);

    gs_render_start(false);
    for (const auto &pt : points_) {
        gs_vertex2f(pt.x, pt.y);
    }
    gs_render_stop(GS_LINESTRIP);
}

bool FreehandMark::hit_test(float /* x */, float /* y */) const
{
    return false;  // Phase 2: implement pick-to-delete
}
