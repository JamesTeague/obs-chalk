#include "circle-mark.hpp"
#include <graphics/graphics.h>
#include <cmath>
#include <limits>

static constexpr float TWO_PI    = 6.28318530718f;
static constexpr int   CIRCLE_PTS = 64;

CircleMark::CircleMark(float cx, float cy, float radius,
                       float r, float g, float b, float a)
    : cx_(cx), cy_(cy), r_(radius)
{
    vec4_set(&color_, r, g, b, a);
}

void CircleMark::update_end(float x, float y)
{
    r_ = std::hypot(x - cx_, y - cy_);
}

void CircleMark::draw(gs_eparam_t *color_param) const
{
    if (r_ < 1.0f)
        return;

    gs_effect_set_vec4(color_param, &color_);

    const float HALF_W = 3.0f;
    gs_render_start(false);
    for (int i = 0; i <= CIRCLE_PTS; ++i) {
        float angle = TWO_PI * static_cast<float>(i) / static_cast<float>(CIRCLE_PTS);
        float cos_a = std::cos(angle), sin_a = std::sin(angle);
        float cx_pt = cx_ + r_ * cos_a;
        float cy_pt = cy_ + r_ * sin_a;
        // Outward radial normal = (cos_a, sin_a)
        gs_vertex2f(cx_pt + cos_a * HALF_W, cy_pt + sin_a * HALF_W);
        gs_vertex2f(cx_pt - cos_a * HALF_W, cy_pt - sin_a * HALF_W);
    }
    gs_render_stop(GS_TRISTRIP);
}

float CircleMark::distance_to(float x, float y) const
{
    return std::abs(std::hypot(x - cx_, y - cy_) - r_);
}
