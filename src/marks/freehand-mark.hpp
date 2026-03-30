#pragma once
#include "mark.hpp"
#include <graphics/vec4.h>
#include <vector>

struct FreehandPoint {
    float x;
    float y;
};

class FreehandMark : public Mark {
public:
    FreehandMark(float r, float g, float b, float a = 1.0f);

    void  add_point(float x, float y) override;
    void  draw(gs_eparam_t *color_param) const override;
    float distance_to(float x, float y) const override;

private:
    std::vector<FreehandPoint> points_;
    vec4 color_;
};
