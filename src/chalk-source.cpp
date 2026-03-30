#include "chalk-source.hpp"
#include <obs-module.h>

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

static void chalk_video_render(void * /* data */, gs_effect_t * /* effect */)
{
    // Set up transparent blend state for correct alpha compositing.
    // No marks to render yet -- establishes the render loop structure.
    gs_blend_state_push();
    gs_enable_blending(true);
    gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

    gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
    gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
    gs_technique_begin(tech);
    gs_technique_end(tech);

    gs_blend_state_pop();
}

static void chalk_mouse_click(void * /* data */,
                              const struct obs_mouse_event * /* event */,
                              int32_t /* type */, bool /* mouse_up */,
                              uint32_t /* click_count */)
{
    // Phase 1 stub -- drawing implemented in Phase 2
}

static void chalk_mouse_move(void * /* data */,
                             const struct obs_mouse_event * /* event */,
                             bool /* mouse_leave */)
{
    // Phase 1 stub -- drawing implemented in Phase 2
}
