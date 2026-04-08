#pragma once
#include "mark.hpp"
#include <graphics/vec4.h>
#include <vector>

struct FreehandPoint {
    float x;
    float y;
    float pressure;  // 0.0-1.0, where 1.0 = full width
};

class FreehandMark : public Mark {
public:
    FreehandMark(float r, float g, float b, float a = 1.0f);

    void  add_point(float x, float y, float pressure = 1.0f) override;
    void  draw(gs_eparam_t *color_param) const override;
    float distance_to(float x, float y) const override;

    static constexpr float CHALK_MIN_WIDTH = 1.5f;  // hairline at zero pressure
    static constexpr float CHALK_MAX_WIDTH = 6.0f;  // full width at max pressure

private:
    std::vector<FreehandPoint> points_;
    vec4 color_;
};
