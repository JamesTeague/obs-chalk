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

    gs_render_start(false);
    for (int i = 0; i <= CIRCLE_PTS; ++i) {
        float angle = TWO_PI * static_cast<float>(i) / static_cast<float>(CIRCLE_PTS);
        gs_vertex2f(cx_ + r_ * std::cos(angle), cy_ + r_ * std::sin(angle));
    }
    gs_render_stop(GS_LINESTRIP);
}

float CircleMark::distance_to(float x, float y) const
{
    return std::abs(std::hypot(x - cx_, y - cy_) - r_);
}
