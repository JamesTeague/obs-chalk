#include "freehand-mark.hpp"
#include <graphics/graphics.h>
#include <algorithm>
#include <cmath>
#include <limits>

FreehandMark::FreehandMark(float r, float g, float b, float a)
{
    vec4_set(&color_, r, g, b, a);
}

void FreehandMark::add_point(float x, float y)
{
    if (!points_.empty()) {
        float dx   = x - points_.back().x;
        float dy   = y - points_.back().y;
        float dist = std::hypot(dx, dy);
        const float STEP = 2.0f;
        if (dist > STEP) {
            float ox = points_.back().x;
            float oy = points_.back().y;
            int n = static_cast<int>(dist / STEP);
            for (int i = 1; i <= n; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(n + 1);
                points_.push_back({ox + dx * t, oy + dy * t});
            }
        }
    }
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

float FreehandMark::distance_to(float x, float y) const
{
    if (points_.size() < 2)
        return std::numeric_limits<float>::max();

    float min_dist = std::numeric_limits<float>::max();

    for (size_t i = 1; i < points_.size(); ++i) {
        float ax = points_[i - 1].x;
        float ay = points_[i - 1].y;
        float bx = points_[i].x;
        float by = points_[i].y;

        float dx   = bx - ax;
        float dy   = by - ay;
        float len2 = dx * dx + dy * dy;

        float dist;
        if (len2 < 1e-6f) {
            dist = std::hypot(x - ax, y - ay);
        } else {
            float t = ((x - ax) * dx + (y - ay) * dy) / len2;
            t = std::clamp(t, 0.0f, 1.0f);
            dist = std::hypot(x - (ax + t * dx), y - (ay + t * dy));
        }

        if (dist < min_dist)
            min_dist = dist;
    }

    return min_dist;
}
