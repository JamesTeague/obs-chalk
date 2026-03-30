#include "arrow-mark.hpp"
#include <graphics/graphics.h>
#include <algorithm>
#include <cmath>
#include <limits>

ArrowMark::ArrowMark(float x1, float y1, float x2, float y2,
                     float r, float g, float b, float a)
    : x1_(x1), y1_(y1), x2_(x2), y2_(y2)
{
    vec4_set(&color_, r, g, b, a);
}

void ArrowMark::update_end(float x, float y)
{
    x2_ = x;
    y2_ = y;
}

void ArrowMark::draw(gs_eparam_t *color_param) const
{
    float dx  = x2_ - x1_;
    float dy  = y2_ - y1_;
    float len = std::hypot(dx, dy);
    if (len < 2.0f)
        return;

    gs_effect_set_vec4(color_param, &color_);

    // Shaft
    gs_render_start(false);
    gs_vertex2f(x1_, y1_);
    gs_vertex2f(x2_, y2_);
    gs_render_stop(GS_LINESTRIP);

    // Arrowhead arms (30 degrees = 0.5236 rad from reversed shaft direction)
    float angle = std::atan2(dy, dx);
    const float ARM_LEN   = 15.0f;
    const float ARM_ANGLE = 0.5236f; // 30 degrees in radians

    // Left arm
    float la_x = x2_ + ARM_LEN * std::cos(angle + static_cast<float>(M_PI) - ARM_ANGLE);
    float la_y = y2_ + ARM_LEN * std::sin(angle + static_cast<float>(M_PI) - ARM_ANGLE);
    gs_render_start(false);
    gs_vertex2f(x2_, y2_);
    gs_vertex2f(la_x, la_y);
    gs_render_stop(GS_LINESTRIP);

    // Right arm
    float ra_x = x2_ + ARM_LEN * std::cos(angle + static_cast<float>(M_PI) + ARM_ANGLE);
    float ra_y = y2_ + ARM_LEN * std::sin(angle + static_cast<float>(M_PI) + ARM_ANGLE);
    gs_render_start(false);
    gs_vertex2f(x2_, y2_);
    gs_vertex2f(ra_x, ra_y);
    gs_render_stop(GS_LINESTRIP);
}

float ArrowMark::distance_to(float x, float y) const
{
    float dx   = x2_ - x1_;
    float dy   = y2_ - y1_;
    float len2 = dx * dx + dy * dy;

    if (len2 < 1e-6f)
        return std::hypot(x - x1_, y - y1_);

    float t = ((x - x1_) * dx + (y - y1_) * dy) / len2;
    t = std::clamp(t, 0.0f, 1.0f);
    return std::hypot(x - (x1_ + t * dx), y - (y1_ + t * dy));
}
