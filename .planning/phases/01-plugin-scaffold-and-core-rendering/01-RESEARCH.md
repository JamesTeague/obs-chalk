# Phase 1: Plugin Scaffold and Core Rendering - Research

**Researched:** 2026-03-29
**Domain:** Native C++17 OBS Studio plugin — source registration, graphics context, object model, thread safety
**Confidence:** HIGH — all critical patterns verified against OBS official docs and source code

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
1. **Object model architecture.** Marks are vector objects, not pixels. This is the core architectural departure from obs-draw and drives most of the benefits.
2. **Native C/C++ OBS plugin.** Direct preview mouse interaction without mode switching. Native hotkey integration. No browser engine overhead. Also a skill-building goal for Teague.
3. **Name is obs-chalk.** Telestrator/telestrate are discoverability keywords, not the name.
4. **Laser pointer is toggle-hold**, not always-on. Hold a key to show, release to hide.
5. **Pick-to-delete mode** for selective mark removal. Combined with most-recent-first undo for quick corrections.
6. **Open source from the start.**

### Claude's Discretion
- Arrow end style variations — types and visual design are implementation detail
- Cone of vision angle defaults and adjustable range
- Color palette choices and number of quick-switch colors
- Hit-testing approach for pick-to-delete (bounding box vs. precise shape testing)
- Undo stack depth
- Tablet pressure mapping (what pressure affects — size, opacity, both)
- Specific hotkey defaults

### Deferred Ideas (OUT OF SCOPE)
- Marks tracking with video movement
- Text/label annotations
- Save/load mark sets
- Multi-source support
- Custom stamp/image tools
- Recording/replay of drawing sequence
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CORE-01 | Plugin registers as OBS input source compositing as transparent overlay | Source registration pattern, OBS_SOURCE_TYPE_INPUT with correct flags, transparent RGBA blend state |
| CORE-02 | Drawing marks are vector objects in a render list, not pixels in a texture buffer | Abstract Mark base class + concrete subclasses, per-frame re-render via gs_render_start/vertex2f/render_stop |
| CORE-03 | Render list is mutex-protected for thread safety between render thread and input callbacks | std::mutex on MarkList, lock in video_render (read) and all write callbacks |
| CORE-04 | All GPU rendering happens exclusively within video_render callback | Confirmed: video_render is already in graphics context; no obs_enter_graphics needed inside it; hotkey/mouse callbacks must never call gs_* |
| CORE-05 | Plugin builds against OBS 31.x+ using obs-plugintemplate and CMake | cmake_minimum_required(3.28...3.30), buildspec.json targeting OBS 31.1.1, Qt6 link against OBS.app frameworks |
</phase_requirements>

---

## Summary

Phase 1 establishes the invariants that every subsequent phase depends on. The three critical invariants are: (1) source type is `OBS_SOURCE_TYPE_INPUT` from the first commit — a filter plugin cannot be independently positioned or receive mouse input as an overlay; (2) all GPU calls are confined to `video_render` — this is the architectural choice that eliminates obs-draw's crash class entirely; (3) the mark list is mutex-protected everywhere it is touched — OBS thread boundaries between render/UI/hotkey callbacks are real and concurrent.

The build foundation uses obs-plugintemplate (cmake 3.28...3.30, buildspec.json targeting OBS 31.1.1), with Qt6 linked against OBS.app's bundled frameworks rather than Homebrew Qt. The CMake flags `ENABLE_FRONTEND_API=ON` and `ENABLE_QT=ON` are opt-in in the template. On macOS the `CMAKE_PREFIX_PATH` must point at `/Applications/OBS.app/Contents/Frameworks` to get the version-matched Qt (currently Qt 6.8.x).

For drawing, OBS provides an immediate-mode API: `gs_render_start(false)` / `gs_vertex2f(x,y)` / `gs_render_stop(GS_LINESTRIP)` for line primitives. Color is set per-vertex via `gs_color(uint32_t)`, and the effect is `obs_get_base_effect(OBS_EFFECT_SOLID)` with a `vec4` color parameter. Phase 1 needs only enough of this to render freehand strokes — proving the render loop works before any other tool is built.

**Primary recommendation:** Get the plugin scaffolding loading in OBS with a transparent overlay first (no drawing), then add the abstract Mark base and a single FreehandMark concrete class to prove the render loop. The threading invariants must be established in this phase; they cannot be retrofitted.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++ | C++17 | Plugin language | Required by Qt6; libobs is C99 but mixing is normal OBS practice |
| CMake | 3.28...3.30 | Build system | Only supported OBS plugin build system; obs-plugintemplate sets explicit range |
| libobs | 31.1.1 (via buildspec) | Core OBS API: source registration, video_render, gs_* graphics, hotkeys, mouse events | This IS the plugin SDK — no separate installer, link against OBS.app frameworks |
| obs-frontend-api | (ships with OBS) | Dock registration, scene event callbacks | Separate from libobs; required for Phase 3 dock; Phase 1 does not need it |
| Qt6 | Must match OBS.app bundle (6.8.x) | Dock panel UI, tablet input | Must match exactly; Homebrew Qt 6.9.x is binary-incompatible with OBS 32.x |
| obs-plugintemplate | main | CMake scaffolding, CI presets | Mandatory starting point; provides CMakePresets.json and GitHub Actions CI |

### Supporting (Phase 1)

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `gs_render_start/stop` | (libobs built-in) | Immediate-mode vector drawing (lines, strips) | Inside `video_render` only; this is how marks are drawn |
| `obs_get_base_effect(OBS_EFFECT_SOLID)` | (libobs built-in) | Solid color rendering for primitives | The standard effect for colored line/shape drawing |
| `std::mutex` | C++17 stdlib | Mark list thread protection | Lock in every read and write path to the mark list |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `OBS_SOURCE_TYPE_INPUT` | `OBS_SOURCE_TYPE_FILTER` | Filter cannot be independently positioned or receive mouse input as an overlay; input source IS the canvas |
| `obs_get_base_effect(OBS_EFFECT_SOLID)` + immediate-mode primitives | Custom .effect shader file | Custom shaders give more visual flexibility (anti-aliasing, soft edges) but add Phase 1 complexity without Phase 1 benefit; use solid effect for scaffold |
| `std::mutex` | Lock-free ring buffer / command queue | Lock-free is more complex and unnecessary at ~10 marks; std::mutex is correct and sufficient |
| Per-frame re-render of all marks | Accumulating marks into a persistent texture | Texture accumulation is obs-draw's architecture — the root cause of its crash; never |

**Installation:**
```bash
# Clone template (use GitHub's "Use this template" or gh CLI)
gh repo create obs-chalk --template obsproject/obs-plugintemplate --public

# macOS build deps (no qt@6 from Homebrew — link OBS.app frameworks instead)
brew install cmake ninja pkg-config

# Set OBS.app framework path before cmake configure
# (add to CMakeLists.txt, see Architecture Patterns below)

# Configure and build
cmake --preset macos-ci
cmake --build build_macos
```

---

## Architecture Patterns

### Recommended Project Structure (Phase 1 subset)

```
src/
├── plugin.cpp              # obs_module_load/unload, source type registration
├── chalk-source.hpp        # ChalkSource class declaration
├── chalk-source.cpp        # obs_source_info callbacks: create/destroy, video_render,
│                           # mouse_click, mouse_move, get_width, get_height
├── mark-list.hpp           # MarkList: vector<unique_ptr<Mark>> + in_progress + mutex
├── mark-list.cpp           # add/commit/clear operations (undo deferred to Phase 2)
├── marks/
│   ├── mark.hpp            # Abstract Mark base: virtual draw(), virtual hit_test()
│   └── freehand-mark.hpp   # Concrete freehand: vector<vec2> points + color
│   └── freehand-mark.cpp   # FreehandMark::draw() implementation
├── tool-state.hpp          # Active tool enum (stub for Phase 1), active color
└── CMakeLists.txt
```

**Phase 1 only needs the files listed above.** `hotkey-manager`, `dock/`, and the other mark types are Phase 2 and Phase 3.

### Pattern 1: OBS Source Registration (CORE-01, CORE-05)

**What:** Register `obs_source_info` struct with `obs_register_source()` in `obs_module_load()`. The struct must use `OBS_SOURCE_TYPE_INPUT` and include the correct output flags.

**When to use:** Always — this is the first code written and it never changes.

**Correct flags:**
```c
// Source: OBS obs-source.h + color-source.c reference
static struct obs_source_info chalk_source_info = {
    .id             = "chalk_drawing_source",
    .type           = OBS_SOURCE_TYPE_INPUT,
    .output_flags   = OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_CUSTOM_DRAW,
    .get_name       = chalk_get_name,
    .create         = chalk_create,
    .destroy        = chalk_destroy,
    .get_width      = chalk_get_width,
    .get_height     = chalk_get_height,
    .video_render   = chalk_video_render,
    .mouse_click    = chalk_mouse_click,
    .mouse_move     = chalk_mouse_move,
};

bool obs_module_load(void)
{
    obs_register_source(&chalk_source_info);
    return true;
}
```

**`OBS_SOURCE_CUSTOM_DRAW` is required:** Without it OBS may apply optimizations that bypass the RGBA alpha compositing needed for a transparent overlay. The `video_render` callback receives `NULL` for the `effect` parameter when this flag is set — the plugin manages its own effect.

**`OBS_SOURCE_INTERACTION` is required:** Without this flag, `mouse_click` and `mouse_move` callbacks never fire.

### Pattern 2: Transparent Overlay Canvas Dimensions (CORE-01)

**What:** `get_width` and `get_height` must return the OBS base canvas resolution so the overlay covers the full scene.

**When to use:** Always — missing or wrong dimensions make the source invisible in OBS.

```cpp
// Source: obs_video_info from libobs/obs.h
uint32_t chalk_get_width(void *data)
{
    struct obs_video_info ovi;
    obs_get_video_info(&ovi);
    return ovi.base_width;
}

uint32_t chalk_get_height(void *data)
{
    struct obs_video_info ovi;
    obs_get_video_info(&ovi);
    return ovi.base_height;
}
```

### Pattern 3: video_render With Object Model (CORE-02, CORE-03, CORE-04)

**What:** The render callback iterates the mutex-protected mark list and draws each mark's geometry using the SOLID effect. No texture accumulation. Clean per-frame render.

**When to use:** Always — this is the entire render loop.

```cpp
// Source: OBS color-source.c pattern + obs-source.h callback contract
static void chalk_video_render(void *data, gs_effect_t *effect)
{
    // effect param is NULL when OBS_SOURCE_CUSTOM_DRAW is set
    UNUSED_PARAMETER(effect);

    ChalkSource *ctx = static_cast<ChalkSource *>(data);

    // Set up transparent alpha blend for overlay compositing
    gs_blend_state_push();
    gs_reset_blend_state();
    gs_enable_blending(true);
    gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

    // Get the solid effect for colored primitive rendering
    gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
    gs_eparam_t *color_param = gs_effect_get_param_by_name(solid, "color");
    gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

    gs_technique_begin(tech);
    gs_technique_begin_pass(tech, 0);

    {   // Lock mark list for read
        std::lock_guard<std::mutex> lock(ctx->mark_list.mutex);
        for (const auto &mark : ctx->mark_list.marks) {
            mark->draw(color_param);
        }
        if (ctx->mark_list.in_progress) {
            ctx->mark_list.in_progress->draw(color_param);
        }
    }   // Mutex released here

    gs_technique_end_pass(tech);
    gs_technique_end(tech);

    gs_blend_state_pop();
}
```

### Pattern 4: FreehandMark draw() Implementation (CORE-02)

**What:** A freehand mark stores a `std::vector<vec2>` of points. Its `draw()` method emits them as a line strip using OBS immediate-mode primitives.

**When to use:** This is the Phase 1 proof-of-concept mark type.

```cpp
// Source: OBS graphics.c gs_render_start/stop implementation
void FreehandMark::draw(gs_eparam_t *color_param) const
{
    if (points_.size() < 2)
        return;

    struct vec4 c;
    vec4_set(&c, color_.r, color_.g, color_.b, color_.a);
    gs_effect_set_vec4(color_param, &c);

    gs_render_start(false);   // false = use OBS immediate vertex buffer (no allocation)
    for (const auto &pt : points_) {
        gs_vertex2f(pt.x, pt.y);
    }
    gs_render_stop(GS_LINESTRIP);
}
```

**Note on `gs_render_start(false)` vs `gs_render_start(true)`:**
- `false` = reuse OBS's built-in immediate vertex buffer (no allocation, use for every frame)
- `true` = allocate a new vertex buffer object (needed for persistent VBOs, not for immediate rendering)

For Phase 1 and the whole object model, `false` is always correct — geometry is rebuilt per frame.

### Pattern 5: Mutex-Protected Mark List (CORE-03)

**What:** `MarkList` owns a `std::mutex`. Every access from any thread takes the lock.

```cpp
// Source: standard C++17 practice confirmed against OBS threading model
struct MarkList {
    std::vector<std::unique_ptr<Mark>> marks;
    std::unique_ptr<Mark>              in_progress;
    mutable std::mutex                 mutex;
};

// Mouse callback: runs on render thread or main thread (undocumented, treat as concurrent)
void chalk_mouse_move(void *data, const obs_mouse_event *event, bool mouse_leave)
{
    ChalkSource *ctx = static_cast<ChalkSource *>(data);
    std::lock_guard<std::mutex> lock(ctx->mark_list.mutex);
    if (ctx->mark_list.in_progress) {
        ctx->mark_list.in_progress->add_point(
            static_cast<float>(event->x),
            static_cast<float>(event->y));
    }
}
```

### Pattern 6: GPU Resource Lifecycle (Pitfall Prevention)

**What:** GPU objects created in `create`, destroyed in `destroy`, both inside `obs_enter_graphics/obs_leave_graphics`.

**When to use:** Whenever any `gs_*` object needs persistent lifetime (effects, textures). For Phase 1, the only GPU resource needing lifecycle management is a pre-loaded effect (optional — `obs_get_base_effect` is free).

```cpp
// Source: OBS pitfall docs + obs-draw source analysis
void *chalk_create(obs_data_t *settings, obs_source_t *source)
{
    auto *ctx = new ChalkSource(source);
    // No GPU resource creation needed in Phase 1 (obs_get_base_effect is free to call)
    // If loading a custom effect file:
    // obs_enter_graphics();
    // ctx->effect = gs_effect_create_from_file("path.effect", nullptr);
    // obs_leave_graphics();
    return ctx;
}

void chalk_destroy(void *data)
{
    auto *ctx = static_cast<ChalkSource *>(data);
    // If any gs_* objects were created:
    // obs_enter_graphics();
    // gs_effect_destroy(ctx->effect);
    // obs_leave_graphics();
    delete ctx;
}
```

### Pattern 7: CMakeLists.txt Qt6 Version Pinning (CORE-05)

**What:** Direct CMake to find Qt6 inside OBS.app's bundled frameworks, not Homebrew.

```cmake
# CMakeLists.txt — add before find_package calls
# Set CMAKE_PREFIX_PATH via environment or CMakePresets.json
# For local macOS development against installed OBS.app:
if(APPLE)
  list(PREPEND CMAKE_PREFIX_PATH "/Applications/OBS.app/Contents/Frameworks")
endif()

# Template flags to enable (edit CMakeLists.txt or use CMakePresets.json):
option(ENABLE_FRONTEND_API "Use obs-frontend-api" ON)   # Phase 1: OFF; Phase 3: ON
option(ENABLE_QT "Use Qt" ON)                           # Phase 1: OFF; Phase 3: ON
```

**Phase 1 does not need Qt or obs-frontend-api.** Enable them when needed (Phase 3). Setting them OFF in Phase 1 reduces build complexity.

### Anti-Patterns to Avoid

- **Pixel buffer rendering (obs-draw pattern):** Never render marks into a persistent `gs_texrender_t` and accumulate strokes. This is the exact crash cause that obs-chalk exists to fix. One `gs_texrender_create` per frame is wasteful; no persistent texrender at all is correct for the object model.
- **Calling gs_* outside video_render:** Never call any `gs_*` function from a hotkey callback, mouse event handler, Qt slot, or frontend event callback. Write to the mark list (CPU state) and let `video_render` handle all GPU work.
- **Nested obs_enter_graphics:** `video_render` is already inside the graphics context. Calling `obs_enter_graphics()` inside it deadlocks. `obs_enter_graphics/leave_graphics` is single-level only.
- **Using OBS_SOURCE_TYPE_FILTER:** A filter cannot be independently positioned in the scene or receive mouse input as a standalone overlay. `OBS_SOURCE_TYPE_INPUT` from the first commit.
- **Calling obs_frontend_add_dock from obs_module_load before post_load:** Frontend API dock calls should be in `obs_module_post_load`, not `obs_module_load`, to ensure OBS frontend is initialized.
- **Skipping get_width/get_height:** Without these, OBS treats the source as 0x0 — it is completely invisible.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Plugin build scaffolding and CI | Custom CMake from scratch | `obs-plugintemplate` | Provides CMakePresets.json, GitHub Actions for all 3 platforms, clang-format, buildspec dependency management |
| Thread-safe mark list | Custom lock-free data structure | `std::mutex` + `std::lock_guard` | Lock-free complexity unjustified at ~10 marks; mutex is correct and readable |
| Solid color shader | Custom .effect file | `obs_get_base_effect(OBS_EFFECT_SOLID)` | Built-in, maintained by OBS team, handles sRGB correctly |
| Per-frame vertex buffer | `gs_vertbuffer_t` lifecycle management | `gs_render_start(false)` immediate mode | `false` parameter reuses OBS's internal buffer — zero allocation overhead, correct for per-frame geometry |
| Module macros | Custom module registration boilerplate | `OBS_DECLARE_MODULE()`, `OBS_MODULE_USE_DEFAULT_LOCALE()` | Required macros provided by obs-module.h; skipping them causes plugin load failure |

**Key insight:** The OBS plugin API provides exactly the primitives needed for this phase. The surface area of "things to build" in Phase 1 is intentionally small — get the invariants right, not feature-complete.

---

## Common Pitfalls

### Pitfall 1: Qt Version Mismatch Causing Silent Load Failure
**What goes wrong:** Plugin loads with no error in OBS, but certain features silently don't work — or at worst, "Cannot mix incompatible Qt library" at runtime causes a crash.
**Why it happens:** Homebrew Qt6 is currently 6.9.x; OBS 32.x bundles Qt 6.8.x. CMake finds whichever Qt is first in the path — usually Homebrew — and links against it.
**How to avoid:** In CMakeLists.txt (or CMakePresets.json), prepend `/Applications/OBS.app/Contents/Frameworks` to `CMAKE_PREFIX_PATH` before any `find_package(Qt6 ...)` call. Phase 1 doesn't need Qt — `ENABLE_QT=OFF` avoids the problem entirely.
**Warning signs:** Plugin loads but behaves unexpectedly; Qt version mismatch message in OBS log.

### Pitfall 2: get_width/get_height Not Implemented
**What goes wrong:** Source is invisible in the OBS preview and does not appear in the scene, even though it loads successfully.
**Why it happens:** `get_width` and `get_height` are required callbacks for synchronous video sources. OBS uses them to allocate compositing space. Without them, OBS defaults to 0x0.
**How to avoid:** Implement both in the first commit using `obs_get_video_info`. Return `ovi.base_width` and `ovi.base_height`. These must return non-zero values for the source to be visible.
**Warning signs:** Source added to scene but nothing appears; no error in OBS log.

### Pitfall 3: OBS_SOURCE_CUSTOM_DRAW Missing
**What goes wrong:** The `video_render` callback receives a non-NULL `effect` argument and OBS may composite the source before the custom rendering, producing incorrect blend behavior or a black background instead of transparent.
**Why it happens:** Without `OBS_SOURCE_CUSTOM_DRAW`, OBS applies default post-processing to the source output. The plugin's alpha compositing conflicts with OBS's default processing.
**How to avoid:** Include `OBS_SOURCE_CUSTOM_DRAW` in `output_flags`. Treat the `effect` parameter in `video_render` as always `NULL`.
**Warning signs:** Drawing overlay has a black or opaque background; marks look correct but overlay isn't transparent.

### Pitfall 4: Calling obs_enter_graphics Inside video_render
**What goes wrong:** OBS freezes (deadlock). The render thread already holds the graphics context when `video_render` fires; calling `obs_enter_graphics()` inside it tries to acquire the same mutex from the same thread.
**Why it happens:** Developer tries to call `obs_enter_graphics` for a sub-operation inside `video_render`, not realizing `video_render` is already inside the graphics context.
**How to avoid:** Never call `obs_enter_graphics` or `obs_leave_graphics` inside `video_render`. All `gs_*` calls inside `video_render` are already in context. `obs_enter_graphics/leave_graphics` is for code outside `video_render` that needs GPU access.
**Warning signs:** OBS hangs (not crashes) when the source is active; stack trace shows graphics thread blocking.

### Pitfall 5: OBS_SOURCE_INTERACTION Missing
**What goes wrong:** Mouse events (`mouse_click`, `mouse_move`) never fire even though the callbacks are registered. Drawing doesn't work.
**Why it happens:** Without the `OBS_SOURCE_INTERACTION` flag in `output_flags`, OBS does not route mouse events to the source's callbacks.
**How to avoid:** Include `OBS_SOURCE_INTERACTION` in `output_flags`. Also: mouse events only reach the source when the user clicks "Interact" in OBS for that source, or when the OBS preview is in interact mode.
**Warning signs:** Callbacks are registered but never called; no marks appear during mouse interaction.

### Pitfall 6: CMake 3.31+ with obs-plugintemplate
**What goes wrong:** CMake configure step fails with version range warnings or errors.
**Why it happens:** obs-plugintemplate explicitly sets `cmake_minimum_required(VERSION 3.28...3.30)`. CMake 3.31+ triggers deprecation warnings because it's outside the stated range.
**How to avoid:** Use CMake 3.30.5 (the version the template CI uses). Check your CMake version: `cmake --version`. Install with `brew install cmake` and pin to 3.30.x if needed.
**Warning signs:** Configure warnings about CMake version compatibility; CI passes but local build fails.

---

## Code Examples

Verified patterns from official OBS source:

### Module Entry Point (plugin.cpp)
```cpp
// Source: obs-module.h + color-source.c reference
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-chalk", "en-US")

extern struct obs_source_info chalk_source_info;  // defined in chalk-source.cpp

bool obs_module_load(void)
{
    obs_register_source(&chalk_source_info);
    return true;
}

void obs_module_unload(void)
{
    // Nothing to clean up in Phase 1
}
```

### Abstract Mark Base Class (marks/mark.hpp)
```cpp
// gs_eparam_t is the handle for the "color" parameter of OBS_EFFECT_SOLID
class Mark {
public:
    virtual ~Mark() = default;
    virtual void draw(gs_eparam_t *color_param) const = 0;
    virtual bool hit_test(float x, float y) const = 0;
};
```

### Complete Solid-Color Line Drawing Pattern
```cpp
// Source: OBS color-source.c (solid effect) + graphics.c (gs_render_start)
// This is the authoritative draw pattern for colored vector marks in OBS

gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

struct vec4 c;
vec4_set(&c, r, g, b, a);
gs_effect_set_vec4(color, &c);

gs_technique_begin(tech);
gs_technique_begin_pass(tech, 0);

gs_render_start(false);
for (auto &pt : points) {
    gs_vertex2f(pt.x, pt.y);
}
gs_render_stop(GS_LINESTRIP);

gs_technique_end_pass(tech);
gs_technique_end(tech);
```

### Mouse Event Struct (from obs-interaction.h)
```c
// Source: libobs/obs-interaction.h — verified from OBS source
struct obs_mouse_event {
    uint32_t modifiers;  // keyboard modifier flags at time of event
    int32_t  x;          // cursor x position (in source pixel coords)
    int32_t  y;          // cursor y position (in source pixel coords)
};

// mouse_click callback signature:
// void (*mouse_click)(void *data, const struct obs_mouse_event *event,
//                     int32_t type,        // button type (left/right/middle)
//                     bool mouse_up,       // true = button release, false = press
//                     uint32_t click_count);

// mouse_move callback signature:
// void (*mouse_move)(void *data, const struct obs_mouse_event *event,
//                    bool mouse_leave);    // true = cursor left source bounds
```

**Important:** `obs_mouse_event` does NOT carry tablet pressure data. Pressure comes from QTabletEvent on the Qt side (Phase 3 concern).

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `obs_frontend_add_dock()` | `obs_frontend_add_dock_by_id()` | OBS 30.0 | Old API is deprecated; new API required for proper dock integration |
| CMake 3.16+ generic | CMake 3.28...3.30 range | obs-plugintemplate circa 2023-2024 | Version range matters — 3.31+ is outside the template's range |
| Qt5 | Qt6 only | OBS 28+; enforced in OBS 32 | OBS 32 refuses to load Qt5 plugins entirely |
| OBS 29.x dev deps | OBS 31.1.1 via buildspec.json | 2025 | Current buildspec.json targets 31.1.1; plugin must build against this version |

**Deprecated/outdated:**
- `obs_frontend_add_dock()`: Deprecated since OBS 30.0. Use `obs_frontend_add_dock_by_id()`.
- Qt5: Dropped. OBS 32 will not load Qt5 plugins.
- CMake below 3.28: obs-plugintemplate rejects it.
- `gs_texrender` double-buffer accumulation (obs-draw pattern): The architecture this project exists to replace.

---

## Open Questions

1. **Exact Qt version bundled with OBS.app on Teague's machine**
   - What we know: OBS 32.x bundles Qt 6.8.x; buildspec.json targets OBS 31.1.1 (which uses an earlier Qt6)
   - What's unclear: Exact Qt 6.8.x subversion to verify at path verification time
   - Recommendation: First task in Phase 1 should be: inspect `/Applications/OBS.app/Contents/Frameworks` on the dev machine and record the exact Qt version before writing any CMakeLists.txt. This is a 30-second verification that prevents hours of debugging.

2. **Mouse event thread origin**
   - What we know: OBS threading guarantees between UI/hotkey/render callbacks are formally underdocumented (confirmed by OBS Discussion #5556)
   - What's unclear: Whether `mouse_click` and `mouse_move` callbacks fire on the render thread or main thread
   - Recommendation: Treat all callbacks as potentially concurrent; always lock the mark list. This is the conservative-correct approach regardless of the answer.

3. **Exact OBS version installed vs buildspec target**
   - What we know: buildspec.json targets OBS 31.1.1; OBS 32.1.0 was released March 2026
   - What's unclear: Whether to target 31.1.1 (conservative) or update buildspec to 32.1.0
   - Recommendation: Start with buildspec.json as shipped (31.1.1). Update to 32.1.0 once the initial scaffold builds successfully and you can verify against the installed OBS version.

---

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | None detected — this is a new C++ OBS plugin project with no existing test infrastructure |
| Config file | None — see Wave 0 |
| Quick run command | `cmake --build build_macos && echo "Build OK"` (build verification as smoke test) |
| Full suite command | Manual OBS load verification (see Phase Gate below) |

**Note:** Unit testing OBS plugin code is difficult because `gs_*` functions require the OBS graphics context. The primary validation method for Phase 1 is build-and-load verification in OBS itself, not unit tests. The render loop, mutex, and mark drawing are best verified by loading the plugin and testing interactively.

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CORE-01 | Plugin appears in OBS "Add Source" list as transparent overlay | Manual OBS load test | `cmake --build build_macos` (build smoke) | ❌ Wave 0 |
| CORE-02 | Freehand stroke drawn with mouse appears in OBS preview | Manual OBS interactive test | `cmake --build build_macos` | ❌ Wave 0 |
| CORE-03 | No crash under concurrent undo + draw operations | Manual stress test | `cmake --build build_macos` | ❌ Wave 0 |
| CORE-04 | No gs_* calls outside video_render (static analysis) | Code review / grep | `grep -rn "gs_" src/ --include="*.cpp" --include="*.hpp"` | ❌ Wave 0 |
| CORE-05 | Plugin binary builds cleanly via CMake | Build CI | `cmake --preset macos-ci && cmake --build build_macos` | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `cmake --build build_macos && echo "Build OK"` — quick build verification
- **Per wave merge:** Full build + manual OBS load test: launch OBS, add source, verify transparent overlay appears
- **Phase gate:** All five Phase 1 success criteria verified manually in OBS before moving to Phase 2

### Wave 0 Gaps
- [ ] `CMakeLists.txt` — with correct cmake version range, ENABLE_FRONTEND_API=OFF, ENABLE_QT=OFF, OBS.app framework path
- [ ] `buildspec.json` — with OBS 31.1.1 dependency hashes (from obs-plugintemplate)
- [ ] `src/plugin.cpp` — module entry point with OBS_DECLARE_MODULE and obs_register_source
- [ ] `src/chalk-source.{hpp,cpp}` — obs_source_info struct, all required callbacks stubbed
- [ ] `src/marks/mark.hpp` — abstract Mark base class
- [ ] `src/mark-list.{hpp,cpp}` — MarkList with std::mutex
- [ ] No unit test framework required for Phase 1; build + OBS load is the validation

---

## Sources

### Primary (HIGH confidence)
- [OBS Source API Reference](https://docs.obsproject.com/reference-sources) — `obs_source_info` callbacks, OBS_SOURCE_* flags, get_width/get_height requirement
- [OBS Plugin Template — GitHub](https://github.com/obsproject/obs-plugintemplate) — cmake_minimum_required(3.28...3.30), ENABLE_QT/ENABLE_FRONTEND_API options, buildspec.json targeting OBS 31.1.1
- [libobs/obs-source.h — GitHub](https://github.com/obsproject/obs-studio/blob/master/libobs/obs-source.h) — obs_source_info struct, all OBS_SOURCE_* flag values and their bit positions
- [libobs/obs-interaction.h — GitHub](https://github.com/obsproject/obs-studio/blob/master/libobs/obs-interaction.h) — obs_mouse_event struct definition (modifiers, x, y)
- [libobs/graphics/graphics.h — GitHub](https://github.com/obsproject/obs-studio/blob/master/libobs/graphics/graphics.h) — gs_render_start, gs_render_stop, gs_vertex2f, gs_color, GS_DRAW_MODE enum
- [libobs/graphics/graphics.c — GitHub](https://github.com/obsproject/obs-studio/blob/master/libobs/graphics/graphics.c) — Confirmed gs_render_start(false) reuses immediate vertex buffer (zero allocation)
- [obs-studio/plugins/image-source/color-source.c](https://github.com/obsproject/obs-studio/blob/master/plugins/image-source/color-source.c) — Authoritative OBS_EFFECT_SOLID + gs_technique_begin/end + gs_draw_sprite pattern
- [libobs/obs.h — GitHub](https://github.com/obsproject/obs-studio/blob/master/libobs/obs.h) — obs_enter_graphics/obs_leave_graphics declarations, obs_get_base_effect with obs_base_effect enum, obs_get_video_info with obs_video_info struct (base_width, base_height)
- [libobs/obs-module.h — GitHub](https://github.com/obsproject/obs-studio/blob/master/libobs/obs-module.h) — OBS_DECLARE_MODULE, OBS_MODULE_USE_DEFAULT_LOCALE, obs_module_load/unload/post_load contracts
- `.planning/research/STACK.md`, `.planning/research/ARCHITECTURE.md`, `.planning/research/PITFALLS.md` — project-level research (2026-03-23), HIGH confidence for threading rules and architecture patterns

### Secondary (MEDIUM confidence)
- [OBS Studio 32.1.0 release — GitHub](https://github.com/obsproject/obs-studio/releases) — Latest stable is 32.1.0 (March 2026); buildspec.json still targets 31.1.1
- [OBS Thread Safety Discussion #5556](https://github.com/obsproject/obs-studio/discussions/5556) — Confirms threading guarantees are underdocumented; conservative locking is correct
- [OBS PR #4576 — OBSQTDisplay destruction race](https://github.com/obsproject/obs-studio/pull/4576) — Confirms Qt display destruction race; avoid embedding preview display in dock (Phase 3 concern)
- [obs-plugintemplate buildspec.json](https://github.com/obsproject/obs-plugintemplate/blob/master/buildspec.json) — OBS 31.1.1 + Qt6 dep dated 2025-07-11

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all core dependencies verified from official sources and obs-plugintemplate
- Architecture: HIGH — source registration patterns, callback signatures, and flag values verified from official OBS headers and color-source.c reference
- Pitfalls: HIGH — GPU threading pitfalls verified from direct obs-draw source analysis and OBS issue tracker; Qt pitfalls deferred to Phase 3
- Drawing pattern: HIGH — OBS_EFFECT_SOLID + gs_technique_begin/end + gs_render_start/stop verified from color-source.c reference and graphics.c implementation

**Research date:** 2026-03-29
**Valid until:** 2026-07-01 (stable OBS plugin API; refresh if OBS major version changes)
