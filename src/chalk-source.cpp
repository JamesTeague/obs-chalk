#include "chalk-source.hpp"
#include "marks/freehand-mark.hpp"
#include "marks/arrow-mark.hpp"
#include "marks/circle-mark.hpp"
#include "marks/cone-mark.hpp"
#include <obs-module.h>
#ifdef ENABLE_FRONTEND_API
#include <obs-frontend-api.h>
#endif
#include <mutex>
#include <cmath>

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
// Shared input dispatch
// ---------------------------------------------------------------------------

// Begin a stroke: create a mark and mark drawing=true.
// Called by both source callbacks (chalk_mouse_click) and ChalkEventFilter.
// pressure: 0.0-1.0 from tablet pen; mouse input uses default 1.0 (full width).
void chalk_input_begin(ChalkSource *ctx, float x, float y, float pressure)
{
    float r, g, b, a;
    color_uint32_to_rgba(ctx->tool_state.active_color(), r, g, b, a);

    // Pick-to-delete mode: remove closest mark (stays active until hotkey toggled off)
    if (ctx->pick_delete_mode) {
        ctx->mark_list.delete_closest(x, y, 20.0f);
        return;
    }

    switch (ctx->tool_state.active_tool) {
        case ToolType::Freehand: {
            auto mark = std::make_unique<FreehandMark>(r, g, b, a);
            mark->add_point(x, y, pressure);
            ctx->mark_list.begin_mark(std::move(mark));
            ctx->drawing = true;
            break;
        }
        case ToolType::Arrow: {
            auto mark = std::make_unique<ArrowMark>(x, y, x, y, r, g, b, a);
            ctx->mark_list.begin_mark(std::move(mark));
            ctx->drawing = true;
            break;
        }
        case ToolType::Circle: {
            auto mark = std::make_unique<CircleMark>(x, y, 0.f, r, g, b, a);
            ctx->mark_list.begin_mark(std::move(mark));
            ctx->drawing = true;
            break;
        }
        case ToolType::Cone: {
            auto mark = std::make_unique<ConeMark>(x, y, x, y, r, g, b, 0.35f);
            ctx->mark_list.begin_mark(std::move(mark));
            ctx->drawing = true;
            break;
        }
        case ToolType::Laser:
            // Laser is not a mark — show dot while mouse button held
            ctx->laser_active = true;
            ctx->laser_x = x;
            ctx->laser_y = y;
            ctx->drawing = true;
            break;
    }
}

// Update a stroke in progress (called on mouse/pen move while drawing).
// pressure: 0.0-1.0 from tablet pen; mouse input uses default 1.0 (full width).
void chalk_input_move(ChalkSource *ctx, float x, float y, float pressure)
{
    std::lock_guard<std::mutex> lock(ctx->mark_list.mutex);
    ctx->laser_x = x;
    ctx->laser_y = y;

    if (ctx->drawing && ctx->mark_list.in_progress) {
        if (ctx->tool_state.active_tool == ToolType::Freehand) {
            ctx->mark_list.in_progress->add_point(x, y, pressure);
        } else {
            ctx->mark_list.in_progress->update_end(x, y);
        }
    }
}

// End a stroke: commit the in-progress mark and clear drawing flag.
// For the Laser tool, hide the dot (commit_mark is a no-op when in_progress is nullptr).
void chalk_input_end(ChalkSource *ctx)
{
    if (ctx->tool_state.active_tool == ToolType::Laser) {
        ctx->laser_active = false;
    }
    ctx->mark_list.commit_mark();
    ctx->drawing = false;
}

// ---------------------------------------------------------------------------
// chalk_find_source — locate the first chalk source in the current scene
// ---------------------------------------------------------------------------

#ifdef ENABLE_FRONTEND_API
namespace {
    struct FindChalkCtx {
        ChalkSource *found = nullptr;
    };

    static bool find_chalk_item(obs_scene_t *, obs_sceneitem_t *item, void *data)
    {
        auto *fctx = static_cast<FindChalkCtx *>(data);
        obs_source_t *src = obs_sceneitem_get_source(item);
        if (!src) return true;
        const char *id = obs_source_get_id(src);
        if (id && strcmp(id, "chalk_drawing_source") == 0) {
            fctx->found = static_cast<ChalkSource *>(obs_obj_get_data(src));
            return false; // stop enumeration
        }
        return true;
    }
} // namespace
#endif

ChalkSource *chalk_find_source()
{
#ifdef ENABLE_FRONTEND_API
    obs_source_t *scene_src = obs_frontend_get_current_scene();
    if (!scene_src) return nullptr;

    obs_scene_t *scene = obs_scene_from_source(scene_src);
    FindChalkCtx fctx;
    if (scene) {
        obs_scene_enum_items(scene, find_chalk_item, &fctx);
    }
    obs_source_release(scene_src);
    return fctx.found;
#else
    return nullptr;
#endif
}

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
// Hotkey callbacks
// ---------------------------------------------------------------------------

// Undo: remove most recent committed mark (MARK-01, MARK-04)
static void chalk_hotkey_undo(void *data, obs_hotkey_id,
                               obs_hotkey_t *, bool pressed)
{
    if (!pressed) return;
    auto *ctx = static_cast<ChalkSource *>(data);
    ctx->mark_list.undo_mark();
}

// Clear all: remove every mark (MARK-03)
static void chalk_hotkey_clear(void *data, obs_hotkey_id,
                                obs_hotkey_t *, bool pressed)
{
    if (!pressed) return;
    auto *ctx = static_cast<ChalkSource *>(data);
    ctx->mark_list.clear_all();
}

// Color cycle: step through palette (INPT-02)
static void chalk_hotkey_color(void *data, obs_hotkey_id,
                                obs_hotkey_t *, bool pressed)
{
    if (!pressed) return;
    auto *ctx = static_cast<ChalkSource *>(data);
    ctx->tool_state.color_index =
        (ctx->tool_state.color_index + 1) % CHALK_PALETTE_SIZE;
}

// Tool selection hotkeys
static void chalk_hotkey_tool_freehand(void *data, obs_hotkey_id,
                                        obs_hotkey_t *, bool pressed)
{
    if (!pressed) return;
    static_cast<ChalkSource *>(data)->tool_state.active_tool = ToolType::Freehand;
}

static void chalk_hotkey_tool_arrow(void *data, obs_hotkey_id,
                                     obs_hotkey_t *, bool pressed)
{
    if (!pressed) return;
    static_cast<ChalkSource *>(data)->tool_state.active_tool = ToolType::Arrow;
}

static void chalk_hotkey_tool_circle(void *data, obs_hotkey_id,
                                      obs_hotkey_t *, bool pressed)
{
    if (!pressed) return;
    static_cast<ChalkSource *>(data)->tool_state.active_tool = ToolType::Circle;
}

static void chalk_hotkey_tool_cone(void *data, obs_hotkey_id,
                                    obs_hotkey_t *, bool pressed)
{
    if (!pressed) return;
    static_cast<ChalkSource *>(data)->tool_state.active_tool = ToolType::Cone;
}

// Pick-to-delete: toggle mode (MARK-02 entry)
static void chalk_hotkey_pick_delete(void *data, obs_hotkey_id,
                                      obs_hotkey_t *, bool pressed)
{
    if (!pressed) return;
    auto *ctx = static_cast<ChalkSource *>(data);
    ctx->pick_delete_mode = !ctx->pick_delete_mode;
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
    auto *ctx = new ChalkSource(source);

    ctx->hotkey_undo = obs_hotkey_register_source(
        source, "chalk.undo", "Chalk: Undo",
        chalk_hotkey_undo, ctx);

    ctx->hotkey_clear = obs_hotkey_register_source(
        source, "chalk.clear", "Chalk: Clear All",
        chalk_hotkey_clear, ctx);

    ctx->hotkey_color = obs_hotkey_register_source(
        source, "chalk.color_next", "Chalk: Next Color",
        chalk_hotkey_color, ctx);

    ctx->hotkey_tool_laser = obs_hotkey_register_source(
        source, "chalk.laser", "Chalk: Laser Pointer",
        chalk_hotkey_laser, ctx);

    ctx->hotkey_tool_freehand = obs_hotkey_register_source(
        source, "chalk.tool.freehand", "Chalk: Freehand",
        chalk_hotkey_tool_freehand, ctx);

    ctx->hotkey_tool_arrow = obs_hotkey_register_source(
        source, "chalk.tool.arrow", "Chalk: Arrow",
        chalk_hotkey_tool_arrow, ctx);

    ctx->hotkey_tool_circle = obs_hotkey_register_source(
        source, "chalk.tool.circle", "Chalk: Circle",
        chalk_hotkey_tool_circle, ctx);

    ctx->hotkey_tool_cone = obs_hotkey_register_source(
        source, "chalk.tool.cone", "Chalk: Cone",
        chalk_hotkey_tool_cone, ctx);

    ctx->hotkey_pick_delete = obs_hotkey_register_source(
        source, "chalk.pick_delete", "Chalk: Pick to Delete",
        chalk_hotkey_pick_delete, ctx);

    return ctx;
}

static void chalk_destroy(void *data)
{
    auto *ctx = static_cast<ChalkSource *>(data);

    // Unregister all hotkeys before destroying context
    obs_hotkey_unregister(ctx->hotkey_undo);
    obs_hotkey_unregister(ctx->hotkey_clear);
    obs_hotkey_unregister(ctx->hotkey_color);
    obs_hotkey_unregister(ctx->hotkey_tool_laser);
    obs_hotkey_unregister(ctx->hotkey_tool_freehand);
    obs_hotkey_unregister(ctx->hotkey_tool_arrow);
    obs_hotkey_unregister(ctx->hotkey_tool_circle);
    obs_hotkey_unregister(ctx->hotkey_tool_cone);
    obs_hotkey_unregister(ctx->hotkey_pick_delete);

    delete ctx;
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

    // Laser pointer: colored circle that follows cursor while hotkey held
    if (ctx->laser_active) {
        float lx, ly;
        {
            std::lock_guard<std::mutex> lock(ctx->mark_list.mutex);
            lx = ctx->laser_x;
            ly = ctx->laser_y;
        }
        float r, g, b, a;
        color_uint32_to_rgba(ctx->tool_state.active_color(), r, g, b, a);
        vec4 laser_color;
        vec4_set(&laser_color, r, g, b, 1.0f);
        gs_effect_set_vec4(color_param, &laser_color);
        const float LASER_RADIUS   = 8.0f;
        const int   LASER_SEGMENTS = 16;
        gs_render_start(false);
        for (int i = 0; i <= LASER_SEGMENTS; ++i) {
            float angle = static_cast<float>(i) / LASER_SEGMENTS
                          * 2.0f * static_cast<float>(M_PI);
            gs_vertex2f(lx + LASER_RADIUS * cosf(angle),
                        ly + LASER_RADIUS * sinf(angle));
        }
        gs_render_stop(GS_LINESTRIP);
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
    const float x = static_cast<float>(event->x);
    const float y = static_cast<float>(event->y);

    // Left button (type 0) — delegate to shared input dispatch
    if (type == 0) {
        if (!mouse_up) {
            chalk_input_begin(ctx, x, y);
        } else {
            chalk_input_end(ctx);
        }
    }

    // Right button (type 2) — clear all (source-interaction-only shortcut)
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
    if (!mouse_leave) {
        const float x = static_cast<float>(event->x);
        const float y = static_cast<float>(event->y);
        chalk_input_move(ctx, x, y);
    }
}
