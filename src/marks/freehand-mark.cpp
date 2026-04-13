#include "freehand-mark.hpp"
#include <graphics/graphics.h>
#include <algorithm>
#include <cmath>
#include <limits>

FreehandMark::FreehandMark(float r, float g, float b, float a)
{
    vec4_set(&color_, r, g, b, a);
}

void FreehandMark::add_point(float x, float y, float pressure)
{
    if (!points_.empty()) {
        float dx   = x - points_.back().x;
        float dy   = y - points_.back().y;
        float dist = std::hypot(dx, dy);
        const float STEP = 2.0f;
        if (dist > STEP) {
            float ox       = points_.back().x;
            float oy       = points_.back().y;
            float op       = points_.back().pressure;
            int   n        = static_cast<int>(dist / STEP);
            for (int i = 1; i <= n; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(n + 1);
                // Lerp position and pressure between previous point and new point
                points_.push_back({ox + dx * t, oy + dy * t, op + (pressure - op) * t});
            }
        }
    }
    points_.push_back({x, y, pressure});
}

void FreehandMark::draw(gs_eparam_t *color_param) const
{
    if (points_.size() < 2)
        return;

    gs_effect_set_vec4(color_param, &color_);

    // OBS immediate-mode rendering has a 512-vertex limit (D3D11 backend).
    // Each point emits 2 vertices, so batch at 250 points (500 verts) max.
    // Overlap by 1 point at batch boundaries to maintain strip continuity.
    static constexpr size_t BATCH_POINTS = 250;

    size_t start = 0;
    while (start < points_.size()) {
        size_t end = std::min(start + BATCH_POINTS, points_.size());
        if (end - start < 2) break;

        gs_render_start(false);

        for (size_t i = start; i < end; ++i) {
            const auto &pt = points_[i];

            float dx, dy;
            if (i + 1 < points_.size()) {
                dx = points_[i + 1].x - pt.x;
                dy = points_[i + 1].y - pt.y;
            } else {
                dx = pt.x - points_[i - 1].x;
                dy = pt.y - points_[i - 1].y;
            }

            float len = std::hypot(dx, dy);
            if (len < 1e-6f) {
                if (i > 0) {
                    dx = pt.x - points_[i - 1].x;
                    dy = pt.y - points_[i - 1].y;
                    len = std::hypot(dx, dy);
                }
                if (len < 1e-6f) {
                    dx = 1.0f;
                    dy = 0.0f;
                    len = 1.0f;
                }
            }

            float px = -dy / len;
            float py =  dx / len;

            float half_w = 0.5f * (CHALK_MIN_WIDTH + pt.pressure * (CHALK_MAX_WIDTH - CHALK_MIN_WIDTH));

            gs_vertex2f(pt.x + px * half_w, pt.y + py * half_w);
            gs_vertex2f(pt.x - px * half_w, pt.y - py * half_w);
        }

        gs_render_stop(GS_TRISTRIP);

        // Overlap by 1 point so the next batch connects seamlessly
        start = end - 1;
    }
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
