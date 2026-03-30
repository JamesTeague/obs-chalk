#pragma once
#include "mark.hpp"
#include <graphics/vec4.h>

class CircleMark : public Mark {
public:
    CircleMark(float cx, float cy, float radius,
               float r, float g, float b, float a = 1.0f);

    void  update_end(float x, float y) override;
    void  draw(gs_eparam_t *color_param) const override;
    float distance_to(float x, float y) const override;

private:
    float cx_, cy_; // center
    float r_;       // radius
    vec4  color_;
};
