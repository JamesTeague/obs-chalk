#pragma once
#include "mark.hpp"
#include <graphics/vec4.h>

class ArrowMark : public Mark {
public:
    ArrowMark(float x1, float y1, float x2, float y2,
              float r, float g, float b, float a = 1.0f);

    void  update_end(float x, float y) override;
    void  draw(gs_eparam_t *color_param) const override;
    float distance_to(float x, float y) const override;

private:
    float x1_, y1_; // tail
    float x2_, y2_; // head
    vec4  color_;
};
