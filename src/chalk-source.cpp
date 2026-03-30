#include "chalk-source.hpp"
#include "marks/freehand-mark.hpp"
#include <obs-module.h>
#include <mutex>

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static const char *chalk_get_name(void *type_data);
static void *chalk_create(obs_data_t *settings, obs_source_t *source);
static void chalk_destroy(void *data);
static uint32_t chalk_get_width(void *data);
static uint32_t chalk_get_height(void *data);
static void chalk_video_render(void *data, gs_effect_t *effect);
static void chalk_mouse_click(void *data,
                              const struct obs_mouse_event *event,
                              int32_t type, bool mouse_up,
                              uint32_t click_count);
static void chalk_mouse_move(void *data,
                             const struct obs_mouse_event *event,
                             bool mouse_leave);

// ---------------------------------------------------------------------------
// obs_source_info registration
// ---------------------------------------------------------------------------

struct obs_source_info chalk_source_info = {
    .id          = "chalk_drawing_source",
    .type        = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO
                  | OBS_SOURCE_INTERACTION
                  | OBS_SOURCE_CUSTOM_DRAW,
    .get_name    = chalk_get_name,
    .create      = chalk_create,
    .destroy     = chalk_destroy,
    .get_width   = chalk_get_width,
    .get_height  = chalk_get_height,
    .video_render = chalk_video_render,
    .mouse_click = chalk_mouse_click,
    .mouse_move  = chalk_mouse_move,
};

// ---------------------------------------------------------------------------
// Color conversion helper
// ---------------------------------------------------------------------------

// Converts ABGR uint32 (OBS convention) to float r,g,b,a components.
static void color_uint32_to_rgba(uint32_t abgr,
                                  float &r, float &g, float &b, float &a)
{
    a = ((abgr >> 24) & 0xFF) / 255.0f;
    b = ((abgr >> 16) & 0xFF) / 255.0f;
    g = ((abgr >> 8)  & 0xFF) / 255.0f;
    r = ((abgr)       & 0xFF) / 255.0f;
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

static const char *chalk_get_name(void * /* type_data */)
{
    return obs_module_text("obs-chalk");
}

static void *chalk_create(obs_data_t * /* settings */, obs_source_t *source)
{
    return new ChalkSource(source);
}

static void chalk_destroy(void *data)
{
    delete static_cast<ChalkSource *>(data);
}

static uint32_t chalk_get_width(void * /* data */)
{
    struct obs_video_info ovi = {};
    obs_get_video_info(&ovi);
    return ovi.base_width;
}

static uint32_t chalk_get_height(void * /* data */)
{
    struct obs_video_info ovi = {};
    obs_get_video_info(&ovi);
    return ovi.base_height;
}

static void chalk_video_render(void *data, gs_effect_t * /* effect */)
{
    auto *ctx = static_cast<ChalkSource *>(data);

    gs_blend_state_push();
    gs_enable_blending(true);
    gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

    gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
    gs_eparam_t *color_param = gs_effect_get_param_by_name(solid, "color");
    gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

    gs_technique_begin(tech);
    gs_technique_begin_pass(tech, 0);

    {
        std::lock_guard<std::mutex> lock(ctx->mark_list.mutex);
        for (const auto &mark : ctx->mark_list.marks) {
            mark->draw(color_param);
        }
        if (ctx->mark_list.in_progress) {
            ctx->mark_list.in_progress->draw(color_param);
        }
    }

    gs_technique_end_pass(tech);
    gs_technique_end(tech);

    gs_blend_state_pop();
}

static void chalk_mouse_click(void *data,
                              const struct obs_mouse_event *event,
                              int32_t type, bool mouse_up,
                              uint32_t /* click_count */)
{
    auto *ctx = static_cast<ChalkSource *>(data);

    // Left button (type 0) — start/commit a freehand stroke
    if (type == 0) {
        if (!mouse_up) {
            // Press: begin a new freehand mark with the active color
            float r, g, b, a;
            color_uint32_to_rgba(ctx->tool_state.active_color(), r, g, b, a);
            auto mark = std::make_unique<FreehandMark>(r, g, b, a);
            mark->add_point(static_cast<float>(event->x),
                            static_cast<float>(event->y));
            ctx->mark_list.begin_mark(std::move(mark));
            ctx->drawing = true;
        } else {
            // Release: commit the in-progress mark to the persistent list
            ctx->mark_list.commit_mark();
            ctx->drawing = false;
        }
    }

    // Right button (type 2) — clear all marks (Phase 1 verification aid)
    if (type == 2 && !mouse_up) {
        ctx->mark_list.clear_all();
        ctx->drawing = false;
    }
}

static void chalk_mouse_move(void *data,
                             const struct obs_mouse_event *event,
                             bool mouse_leave)
{
    auto *ctx = static_cast<ChalkSource *>(data);

    if (ctx->drawing && !mouse_leave) {
        std::lock_guard<std::mutex> lock(ctx->mark_list.mutex);
        if (ctx->mark_list.in_progress) {
            ctx->mark_list.in_progress->add_point(
                static_cast<float>(event->x),
                static_cast<float>(event->y));
        }
    }
}
