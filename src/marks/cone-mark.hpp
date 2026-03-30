#pragma once
#include "mark.hpp"
#include <graphics/vec4.h>

class ConeMark : public Mark {
public:
    ConeMark(float ax, float ay, float cx, float cy,
             float r, float g, float b, float a = 0.35f);

    void  update_end(float x, float y) override;
    void  draw(gs_eparam_t *color_param) const override;
    float distance_to(float x, float y) const override;

private:
    float ax_, ay_;          // apex (click point)
    float cx_, cy_;          // dragged corner (corner1)
    float axis_ux_, axis_uy_; // unit axis direction, locked after first update_end
    bool  axis_set_;
    vec4  color_;

    void compute_corner2(float &c2x, float &c2y) const;
};
