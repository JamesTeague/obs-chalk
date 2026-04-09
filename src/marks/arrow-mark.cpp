#include "arrow-mark.hpp"
#include <graphics/graphics.h>
#include <algorithm>
#include <cmath>
#include <limits>

static void draw_thick_segment(float x1, float y1, float x2, float y2, float half_w)
{
    float dx = x2 - x1, dy = y2 - y1;
    float len = std::hypot(dx, dy);
    if (len < 1e-6f) return;
    float px = -dy / len, py = dx / len;
    gs_render_start(false);
    gs_vertex2f(x1 + px * half_w, y1 + py * half_w);
    gs_vertex2f(x1 - px * half_w, y1 - py * half_w);
    gs_vertex2f(x2 + px * half_w, y2 + py * half_w);
    gs_vertex2f(x2 - px * half_w, y2 - py * half_w);
    gs_render_stop(GS_TRISTRIP);
}

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

    const float HALF_W = 3.0f;

    // Shaft
    draw_thick_segment(x1_, y1_, x2_, y2_, HALF_W);

    // Arrowhead arms (30 degrees = 0.5236 rad from reversed shaft direction)
    float angle = std::atan2(dy, dx);
    const float ARM_LEN   = 15.0f;
    const float ARM_ANGLE = 0.5236f; // 30 degrees in radians

    // Left arm
    float la_x = x2_ + ARM_LEN * std::cos(angle + static_cast<float>(M_PI) - ARM_ANGLE);
    float la_y = y2_ + ARM_LEN * std::sin(angle + static_cast<float>(M_PI) - ARM_ANGLE);
    draw_thick_segment(x2_, y2_, la_x, la_y, HALF_W);

    // Right arm
    float ra_x = x2_ + ARM_LEN * std::cos(angle + static_cast<float>(M_PI) + ARM_ANGLE);
    float ra_y = y2_ + ARM_LEN * std::sin(angle + static_cast<float>(M_PI) + ARM_ANGLE);
    draw_thick_segment(x2_, y2_, ra_x, ra_y, HALF_W);
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
