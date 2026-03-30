#pragma once
#include <graphics/graphics.h>

class Mark {
public:
    virtual ~Mark() = default;
    virtual void draw(gs_eparam_t *color_param) const = 0;
    virtual bool hit_test(float x, float y) const = 0;
    virtual void add_point(float /*x*/, float /*y*/) {}  // default no-op, overridden by freehand
};
