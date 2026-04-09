#include "cone-mark.hpp"
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

ConeMark::ConeMark(float ax, float ay, float cx, float cy,
                   float r, float g, float b, float a)
    : ax_(ax), ay_(ay), cx_(cx), cy_(cy),
      axis_ux_(0.0f), axis_uy_(0.0f), axis_set_(false)
{
    vec4_set(&color_, r, g, b, a);
}

void ConeMark::update_end(float x, float y)
{
    if (!axis_set_) {
        float dx  = x - ax_;
        float dy  = y - ay_;
        float len = std::hypot(dx, dy);
        if (len > 5.0f) {
            axis_ux_  = dx / len;
            axis_uy_  = dy / len;
            axis_set_ = true;
        }
    }
    cx_ = x;
    cy_ = y;
}

void ConeMark::compute_corner2(float &c2x, float &c2y) const
{
    // Reflect corner1 (cx_,cy_) across the axis line through apex (ax_,ay_)
    float dx    = cx_ - ax_;
    float dy    = cy_ - ay_;
    float along = dx * axis_ux_ + dy * axis_uy_;
    float foot_x = ax_ + along * axis_ux_;
    float foot_y = ay_ + along * axis_uy_;
    c2x = 2.0f * foot_x - cx_;
    c2y = 2.0f * foot_y - cy_;
}

void ConeMark::draw(gs_eparam_t *color_param) const
{
    if (!axis_set_)
        return;

    float dx  = cx_ - ax_;
    float dy  = cy_ - ay_;
    float len = std::hypot(dx, dy);
    if (len < 2.0f)
        return;

    // Project corner1 onto axis to get depth; skip if behind apex
    float depth = dx * axis_ux_ + dy * axis_uy_;
    if (depth < 1.0f)
        return;

    float c2x, c2y;
    compute_corner2(c2x, c2y);

    gs_effect_set_vec4(color_param, &color_);

    gs_set_cull_mode(GS_NEITHER);
    gs_render_start(false);
    gs_vertex2f(ax_, ay_);
    gs_vertex2f(cx_, cy_);
    gs_vertex2f(c2x, c2y);
    gs_render_stop(GS_TRIS);
    gs_set_cull_mode(GS_BACK);

    // Side edges at 6px width
    const float HALF_W = 3.0f;
    draw_thick_segment(ax_, ay_, cx_, cy_, HALF_W);   // apex to corner1
    draw_thick_segment(ax_, ay_, c2x, c2y, HALF_W);   // apex to corner2
}

// Point-to-segment distance helper
static float seg_dist(float px, float py,
                      float ax, float ay, float bx, float by)
{
    float dx   = bx - ax;
    float dy   = by - ay;
    float len2 = dx * dx + dy * dy;
    if (len2 < 1e-6f)
        return std::hypot(px - ax, py - ay);
    float t = ((px - ax) * dx + (py - ay) * dy) / len2;
    t = std::clamp(t, 0.0f, 1.0f);
    return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

// Cross product of 2D vectors AB and AP
static float cross2d(float ax, float ay, float bx, float by,
                     float px, float py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

float ConeMark::distance_to(float x, float y) const
{
    if (!axis_set_)
        return std::numeric_limits<float>::max();

    float c2x, c2y;
    compute_corner2(c2x, c2y);

    // Inside-triangle check via cross-product signs
    float d1 = cross2d(ax_, ay_, cx_, cy_,   x, y);
    float d2 = cross2d(cx_, cy_, c2x, c2y,   x, y);
    float d3 = cross2d(c2x, c2y, ax_, ay_,   x, y);

    bool has_neg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    bool has_pos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
    if (!(has_neg && has_pos))
        return 0.0f; // inside triangle

    // Distance to nearest edge
    float e1 = seg_dist(x, y, ax_,  ay_,  cx_,  cy_);
    float e2 = seg_dist(x, y, ax_,  ay_,  c2x,  c2y);
    float e3 = seg_dist(x, y, cx_,  cy_,  c2x,  c2y);
    return std::min({e1, e2, e3});
}
