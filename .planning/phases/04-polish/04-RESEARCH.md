# Phase 4: Polish - Research

**Researched:** 2026-04-08
**Domain:** OBS plugin C++ — rendering, source settings, scene-transition hooks
**Confidence:** HIGH (all findings from direct codebase inspection; no external library unknowns)

## Summary

Phase 4 is a pure-refactor phase with zero new architectural concepts. Every change targets existing code with clear, localized edits. The codebase is small (~500 lines across 10 files), well-structured, and the root cause of each issue is already understood from the CONTEXT.md discussion.

The five changes map cleanly to five independent work items: (1) line width for arrow/circle/cone via triangle strip rendering, (2) cone alpha fix by passing 0.35f instead of palette alpha in `chalk_source.cpp`, (3) persistent delete mode by removing one line in `chalk_input_begin`, (4) laser tool refactor from hold-key to selectable tool via `chalk_input_begin`/`chalk_input_end`, and (5) scene-transition clear via OBS source settings + `obs_frontend_add_event_callback`.

**Primary recommendation:** Implement all five changes as separate, atomic commits. Each is self-contained with no cross-dependencies except laser (which touches both `chalk-source.hpp`, `chalk-source.cpp`, and event filter's mouse-release handling).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Line width:** Arrow, circle, and cone edges render at 6px. Freehand constants: CHALK_MIN_WIDTH=1.5, CHALK_MAX_WIDTH=6.0 in freehand-mark.hpp. Non-freehand marks must render at 6px to match.
- **Cone opacity fix:** Pass 0.35f (or tuned value) instead of palette alpha when creating ConeMark. Root cause is in `chalk_source.cpp` — palette alpha 0xFF (1.0) overrides ConeMark's default. Blend state, shader, pipeline are all correct.
- **Delete mode:** Remove `ctx->pick_delete_mode = false` auto-exit after deletion. Mode stays active until hotkey toggles it off.
- **Laser pointer:** Becomes a selectable tool via hotkey (like freehand/arrow/circle/cone). Shows on mouse-down, hides on mouse-up. Requires chalk mode active. Visual unchanged: filled circle 8px radius, current color, full opacity.
- **Scene-transition clear (INTG-01):** Optional, off by default, configurable in source settings.

### Claude's Discretion
- Exact cone alpha value (start with 0.35, tune if needed)
- Whether line width for non-freehand marks uses triangle strip rendering or gs line width API (prefer whatever gives consistent cross-GPU results)
- How laser tool integrates into ToolState/ToolType enum and tool cycling

### Deferred Ideas (OUT OF SCOPE)
- Base edge removal on cone: evaluate after opacity fix, only pursue if still visually problematic
- Full clickable toolbar: v1.1
- Stream deck integration: future, needs research
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| UX-01 | Arrow, circle, and cone edge marks render at ~6px visual weight | Triangle strip rendering (same as FreehandMark) vs. gs_linewidth; see Architecture Patterns |
| UX-02 | Cone fill is visibly transparent with canvas content visible through interior | ConeMark constructor called with `a` from palette (0xFF = 1.0f); fix is to hardcode 0.35f at call site in chalk_source.cpp |
| UX-03 | Delete mode stays active across multiple clicks until explicitly toggled off | Single line removal: `ctx->pick_delete_mode = false;` in chalk_input_begin |
| UX-04 | Laser pointer is a selectable tool; mouse-down shows dot, mouse-up hides it | Laser moves from standalone flag + hold-key into ToolType enum integration; laser_active driven by drawing flag |
| INTG-01 | Optional clear-on-scene-transition, off by default, configurable in source settings | OBS source settings API: obs_data_t, get_defaults, update callbacks; frontend event OBS_FRONTEND_EVENT_SCENE_CHANGED |
</phase_requirements>

## Standard Stack

### Core (already in use — no new dependencies)
| API | Version | Purpose | Notes |
|-----|---------|---------|-------|
| OBS libobs graphics | 31.x+ | gs_render_start/stop, GS_TRISTRIP | All gs_* calls confined to chalk_video_render |
| OBS obs-frontend-api | 31.x+ | obs_frontend_add_event_callback, obs_frontend_get_current_scene | Already in plugin.cpp |
| obs_data_t | 31.x+ | Source settings persistence | Not yet used; needed for INTG-01 |

**Installation:** No new packages. All APIs are available through the existing OBS plugin template linkage.

## Architecture Patterns

### Recommended Project Structure (unchanged)
```
src/
├── plugin.cpp            # module load/unload, event callback
├── chalk-source.cpp/.hpp # source callbacks, hotkeys, input dispatch
├── chalk-mode.cpp/.hpp   # Qt event filter, preview interaction
├── mark-list.cpp/.hpp    # mark storage, mutex-protected operations
├── tool-state.hpp        # ToolType enum, palette, ToolState struct
└── marks/
    ├── mark.hpp           # base class
    ├── freehand-mark.*    # triangle strip, pressure
    ├── arrow-mark.*       # currently GS_LINESTRIP — needs upgrade
    ├── circle-mark.*      # currently GS_LINESTRIP — needs upgrade
    └── cone-mark.*        # currently GS_TRIS fill — needs edge upgrade
```

### Pattern 1: Triangle Strip for Fixed-Width Lines (UX-01)

**What:** Render a thick line as a rectangle via GS_TRISTRIP. Perpendicular offsets of half_w on each side of each vertex produce consistent 6px width regardless of GPU line width limits.

**When to use:** Any mark that needs consistent, cross-GPU line width. GS_LINESTRIP width is GPU/driver dependent — many GPUs cap at 1px in core OpenGL profile.

**FreehandMark reference implementation (source: freehand-mark.cpp lines 41-88):**
```cpp
// For a segment from P1 to P2, compute perpendicular unit vector:
float dx = p2.x - p1.x, dy = p2.y - p1.y;
float len = std::hypot(dx, dy);
float px = -dy / len, py = dx / len;  // perp unit
float half_w = 3.0f;  // 6px total for non-pressure marks

gs_render_start(false);
gs_vertex2f(p1.x + px * half_w, p1.y + py * half_w);
gs_vertex2f(p1.x - px * half_w, p1.y - py * half_w);
gs_vertex2f(p2.x + px * half_w, p2.y + py * half_w);
gs_vertex2f(p2.x - px * half_w, p2.y - py * half_w);
gs_render_stop(GS_TRISTRIP);
```

**For ArrowMark:** The shaft is a single segment (4 vertices). The two arrowhead arms are also single segments. Each gets its own gs_render_start/stop with GS_TRISTRIP.

**For CircleMark:** 64 segments. Each segment becomes a quad (2 vertices per endpoint = 4 per segment, but strip optimization: 64 × 2 + 2 = 130 vertices total using a single continuous strip).

**Continuous strip for CircleMark:**
```cpp
gs_render_start(false);
for (int i = 0; i <= CIRCLE_PTS; ++i) {
    float angle = TWO_PI * i / CIRCLE_PTS;
    float cx_pt = cx_ + r_ * cosf(angle);
    float cy_pt = cy_ + r_ * sinf(angle);
    // Radial perp direction
    float nx = cosf(angle), ny = sinf(angle);  // outward normal
    gs_vertex2f(cx_pt + nx * half_w, cy_pt + ny * half_w);
    gs_vertex2f(cx_pt - nx * half_w, cy_pt - ny * half_w);
}
gs_render_stop(GS_TRISTRIP);
```

**For ConeMark edges:** Cone currently draws its fill as GS_TRIS (unchanged). The two side edges (apex to corner1, apex to corner2) need triangle strip rendering at 6px. The base edge (corner1 to corner2) is deferred per CONTEXT.md.

### Pattern 2: OBS Source Settings for INTG-01

**What:** Source settings are persisted obs_data_t. Define defaults in `get_defaults`, read in `create` and `update`, expose in `get_properties`.

**OBS source_info callbacks needed:**
```cpp
// In obs_source_info struct:
.get_defaults  = chalk_get_defaults,   // set "clear_on_scene_change" = false
.get_properties = chalk_get_properties, // expose checkbox to user
.update         = chalk_update,         // read settings into ctx->clear_on_scene_change
```

**chalk_get_defaults pattern:**
```cpp
static void chalk_get_defaults(obs_data_t *settings) {
    obs_data_set_default_bool(settings, "clear_on_scene_change", false);
}
```

**chalk_update pattern:**
```cpp
static void chalk_update(void *data, obs_data_t *settings) {
    auto *ctx = static_cast<ChalkSource *>(data);
    ctx->clear_on_scene_change = obs_data_get_bool(settings, "clear_on_scene_change");
}
```

**chalk_get_properties pattern:**
```cpp
static obs_properties_t *chalk_get_properties(void * /* data */) {
    obs_properties_t *props = obs_properties_create();
    obs_properties_add_bool(props, "clear_on_scene_change",
                            obs_module_text("ClearOnSceneChange"));
    return props;
}
```

**Scene transition hook:** The correct event is `OBS_FRONTEND_EVENT_SCENE_CHANGED`. This fires after a scene transition completes. The callback is already pattern-matched in `plugin.cpp`'s `on_obs_event`:
```cpp
// In plugin.cpp on_obs_event:
else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
    // Enumerate chalk sources in the new scene and clear if enabled
    // OR: iterate all registered ChalkSource instances
}
```

**Finding the chalk source from a frontend event:** The existing `chalk_find_source()` in `chalk-source.cpp` already does this — it calls `obs_frontend_get_current_scene()` and enumerates items. On `SCENE_CHANGED`, the current scene has already transitioned, so `chalk_find_source()` finds the chalk source in the new scene.

**Simpler alternative:** Register the clear callback directly on the source. Since `on_obs_event` receives no per-source data, a global list of registered ChalkSource pointers or calling `chalk_find_source()` is needed. The existing `chalk_find_source()` pattern works cleanly.

### Pattern 3: Persistent Delete Mode (UX-03)

**What:** Remove one line from `chalk_input_begin` in `chalk-source.cpp`.

**Current code (lines 40-44):**
```cpp
if (ctx->pick_delete_mode) {
    ctx->mark_list.delete_closest(x, y, 20.0f);
    ctx->pick_delete_mode = false;  // ← REMOVE THIS LINE
    return;
}
```

**After fix:** Mode persists until the toggle hotkey fires. `chalk_hotkey_pick_delete` already implements a toggle (`!ctx->pick_delete_mode`), so no changes needed there.

### Pattern 4: Laser as Selectable Tool (UX-04)

**Current state:** `laser_active` is a separate bool, driven by a hold-key hotkey (`chalk_hotkey_laser`). Laser is in `ToolType::Laser` but the `chalk_input_begin` Laser arm only updates position, does not set `laser_active`.

**Target state:** Selecting laser tool (via `ToolType::Laser`) + mouse-down activates laser dot; mouse-up hides it. The `laser_active` flag becomes driven by `drawing` when active tool is Laser.

**Changes needed:**

1. `chalk-source.hpp`: Remove `hotkey_laser` field (or repurpose — see note below).

2. `chalk-source.cpp` `chalk_input_begin` Laser arm:
```cpp
case ToolType::Laser:
    ctx->laser_active = true;   // show dot on mouse-down
    ctx->laser_x = x;
    ctx->laser_y = y;
    ctx->drawing = true;        // keep drawing=true so move/end work
    break;
```

3. `chalk-source.cpp` `chalk_input_end`:
```cpp
void chalk_input_end(ChalkSource *ctx) {
    if (ctx->tool_state.active_tool == ToolType::Laser) {
        ctx->laser_active = false;   // hide dot on mouse-up
    }
    ctx->mark_list.commit_mark();
    ctx->drawing = false;
}
```

4. `chalk-source.cpp` `chalk_input_move` Laser arm: Already updates `laser_x/laser_y` in the general case (lines 87-97 update position unconditionally). No change needed.

5. Hotkey: `hotkey_laser` was a hold-key. Since Laser is now a tool, the existing `hotkey_tool_*` pattern handles selection. The old `hotkey_laser` should either be removed or repurposed. Per AI discretion, the cleanest path is to remove `hotkey_laser` registration and unregistration from `chalk_create`/`chalk_destroy` and remove the field from `chalk-source.hpp`.

6. `chalk-mode.cpp` event filter: Mouse events dispatch through `chalk_input_begin/move/end` already — no changes needed there. The event filter already passes all Qt mouse events to those functions.

**Note on commit_mark with Laser:** `chalk_input_end` currently calls `ctx->mark_list.commit_mark()`. When `in_progress` is nullptr (as it is for Laser — nothing is ever put in `in_progress`), `commit_mark` is a no-op (`if (in_progress)` guard in mark-list.cpp line 13). Safe.

### Anti-Patterns to Avoid

- **Using `gs_linewidth()`:** Not reliable in OBS's OpenGL/Direct3D abstraction. Core profile OpenGL ignores line widths > 1. Use triangle strip.
- **Modifying `drawing` flag without symmetry:** The `drawing` flag must be set in `begin` and cleared in `end`. The Laser case must follow this or mouse-move will stop updating position.
- **Calling `obs_data_*` outside create/update/get_defaults:** Settings are only valid in those callbacks. Don't cache the obs_data_t pointer.
- **Registering scene event in source create:** Scene transitions are a module-level event, not a source-level event. Register in `plugin.cpp`'s `on_obs_event`, not in `chalk_create`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Persistent settings | Custom config file | `obs_data_t` + get_defaults/update/get_properties | OBS serializes/deserializes automatically with scene collections |
| Scene change detection | Polling loop or Qt timer | `OBS_FRONTEND_EVENT_SCENE_CHANGED` via existing `obs_frontend_add_event_callback` | Already hooked in plugin.cpp |
| Cross-GPU thick lines | `gs_linewidth()` | Triangle strip (GS_TRISTRIP) | GPU line width cap is 1px on many systems |

## Common Pitfalls

### Pitfall 1: Cone alpha — palette vs. default
**What goes wrong:** ConeMark constructor has `a = 0.35f` as default, but `chalk_input_begin` extracts alpha from the palette color via `color_uint32_to_rgba`, which returns `a = 1.0f` for palette entries (all have 0xFF alpha byte). This `a` is passed explicitly as the 8th argument to `ConeMark(...)`, bypassing the default.

**How to fix:** In `chalk_source.cpp` `chalk_input_begin` Cone case, pass `0.35f` (or the desired transparency value) explicitly instead of `a`:
```cpp
case ToolType::Cone: {
    auto mark = std::make_unique<ConeMark>(x, y, x, y, r, g, b, 0.35f);
    // ...
}
```

**Warning signs:** If cone appears fully opaque, check the ConeMark constructor call site, not the constructor itself.

### Pitfall 2: Triangle strip winding for arrowhead arms
**What goes wrong:** Arrowhead arms are short segments. At 6px width, a very short arm can produce a degenerate quad where vertices cross (negative area triangle). This produces visual glitches.

**How to avoid:** Add a minimum arm length check before rendering (arm length already uses ARM_LEN = 15.0f, so it's always long enough). The perpendicular direction computation `len < 1e-6f` guard already exists in FreehandMark — apply the same guard to arrow/circle/cone.

### Pitfall 3: Laser active state leaking across tool switches
**What goes wrong:** User switches to Laser, starts drawing, then switches tool mid-stroke. `laser_active` stays true but the new tool's marks aren't being drawn.

**How to avoid:** Tool switch hotkeys fire on key-down. A mid-stroke tool switch is unlikely in practice. However, if a tool switch fires while `drawing = true`, `chalk_input_end` won't see `ToolType::Laser` anymore and won't clear `laser_active`. Safe defensive fix: clear `laser_active` whenever active tool changes away from Laser in the tool hotkey callbacks, OR check `laser_active` against active tool in `chalk_video_render` (only show if active_tool == Laser).

**Simplest fix for render:** In `chalk_video_render`, guard laser rendering:
```cpp
if (ctx->laser_active && ctx->tool_state.active_tool == ToolType::Laser) {
```

### Pitfall 4: obs_data_t settings callback order
**What goes wrong:** `chalk_create` is called before `chalk_update` in some OBS versions. If `create` doesn't read initial settings, `clear_on_scene_change` could be uninitialized.

**How to avoid:** Read settings in both `chalk_create` (from the settings argument already passed to it) and `chalk_update`. Or simply call the same helper from both:
```cpp
static void *chalk_create(obs_data_t *settings, obs_source_t *source) {
    auto *ctx = new ChalkSource(source);
    chalk_update(ctx, settings);  // initialize from settings immediately
    // ... register hotkeys ...
    return ctx;
}
```

### Pitfall 5: Scene change fires before ChalkSource is in new scene
**What goes wrong:** `OBS_FRONTEND_EVENT_SCENE_CHANGED` fires after transition. `chalk_find_source()` looks in the new scene. If the chalk source is only in the old scene, `chalk_find_source()` returns nullptr.

**How to avoid:** This is expected behavior — if chalk isn't in the new scene, there's nothing to clear. Null-check the result of `chalk_find_source()` (already done everywhere it's called).

### Pitfall 6: CircleMark triangle strip — closing the loop
**What goes wrong:** A circle rendered as a triangle strip requires the last segment to connect back to the first point, or a visible gap appears at the closure point.

**How to avoid:** The existing `GS_LINESTRIP` circle closes the loop by running from `i=0` to `i=CIRCLE_PTS` (64+1 = 65 iterations, returning to angle=0). The same `i <= CIRCLE_PTS` range works for the triangle strip — the first two vertices repeat at i=64, closing the quad strip back to the start.

## Code Examples

### UX-01: Arrow shaft as 6px triangle strip
```cpp
// In ArrowMark::draw, replace gs_render_start/stop for shaft:
float dx = x2_ - x1_, dy = y2_ - y1_;
float len = std::hypot(dx, dy);
if (len < 2.0f) return;
float px = -dy / len, py = dx / len;  // perp unit
const float HALF_W = 3.0f;            // 6px total

gs_render_start(false);
gs_vertex2f(x1_ + px * HALF_W, y1_ + py * HALF_W);
gs_vertex2f(x1_ - px * HALF_W, y1_ - py * HALF_W);
gs_vertex2f(x2_ + px * HALF_W, y2_ + py * HALF_W);
gs_vertex2f(x2_ - px * HALF_W, y2_ - py * HALF_W);
gs_render_stop(GS_TRISTRIP);
```

### UX-02: Cone alpha fix (call site only)
```cpp
// In chalk-source.cpp chalk_input_begin, Cone case:
case ToolType::Cone: {
    auto mark = std::make_unique<ConeMark>(x, y, x, y, r, g, b, 0.35f);
    ctx->mark_list.begin_mark(std::move(mark));
    ctx->drawing = true;
    break;
}
```

### INTG-01: Source settings skeleton
```cpp
// chalk-source.cpp additions:
static void chalk_get_defaults(obs_data_t *settings) {
    obs_data_set_default_bool(settings, "clear_on_scene_change", false);
}

static obs_properties_t *chalk_get_properties(void * /* data */) {
    obs_properties_t *props = obs_properties_create();
    obs_properties_add_bool(props, "clear_on_scene_change",
                            obs_module_text("ClearOnSceneChange"));
    return props;
}

static void chalk_update(void *data, obs_data_t *settings) {
    auto *ctx = static_cast<ChalkSource *>(data);
    ctx->clear_on_scene_change =
        obs_data_get_bool(settings, "clear_on_scene_change");
}

// chalk-source.hpp: add field
bool clear_on_scene_change = false;
```

```cpp
// plugin.cpp on_obs_event addition:
else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
    ChalkSource *ctx = chalk_find_source();
    if (ctx && ctx->clear_on_scene_change) {
        ctx->mark_list.clear_all();
    }
}
```

## State of the Art

| Old Approach | Current Approach | Notes |
|--------------|------------------|-------|
| gs_linewidth for thick lines | GS_TRISTRIP perpendicular offset | FreehandMark already uses tristrip; others still use LINESTRIP |
| Hold-key laser | Selectable laser tool | Phase 4 change |
| Single-shot delete | Persistent delete mode | Phase 4 change |
| No settings | obs_data_t source settings | Phase 4 addition for INTG-01 |

**Currently using GS_LINESTRIP (to be replaced):**
- ArrowMark::draw — shaft + both arrowhead arms (3 separate render calls)
- CircleMark::draw — full circle loop

## Open Questions

1. **Cone base edge after alpha fix**
   - What we know: CONTEXT.md deferred this — evaluate after opacity fix
   - What's unclear: Whether 0.35f alpha makes the base edge invisible enough naturally
   - Recommendation: Implement opacity fix first, note the base edge evaluation as a verification step in the plan

2. **Laser hotkey fate**
   - What we know: `hotkey_laser` is registered as "Chalk: Laser Pointer" and triggers `chalk_hotkey_laser` (sets `laser_active = pressed`)
   - What's unclear: Whether removing it breaks any user-bound hotkey assignments
   - Recommendation: Remove the laser hotkey entirely (it's incompatible with the new model). Document this as a breaking change in the task. Existing tool-selection hotkeys cover laser selection.

3. **Laser position when chalk_mode is off**
   - What we know: `laser_x/laser_y` are updated on every mouse move regardless of tool
   - What's unclear: When laser tool is selected and chalk mode is inactive, the dot is invisible (laser_active=false), so position tracking is harmless
   - Recommendation: No change needed. The existing behavior is correct.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None detected — this is a C++ OBS plugin with no automated test infrastructure |
| Config file | none |
| Quick run command | `cmake --build build/macos --config RelWithDebInfo 2>&1 \| tail -5` |
| Full suite command | Build + manual OBS load verification |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| UX-01 | Arrow/circle/cone edges render at 6px | manual | build clean | ❌ |
| UX-02 | Cone fill visibly transparent | manual | build clean | ❌ |
| UX-03 | Delete mode persists after click | manual | build clean | ❌ |
| UX-04 | Laser shows on mouse-down, hides on mouse-up | manual | build clean | ❌ |
| INTG-01 | Clear-on-scene-transition works when enabled, no-op when disabled | manual | build clean | ❌ |

All tests are manual-only: this is a visual rendering plugin that requires OBS running with a live scene. The OBS plugin sandbox makes automated behavioral testing impractical at v1.

### Sampling Rate
- **Per task commit:** `cmake --build build/macos --config RelWithDebInfo 2>&1 | tail -5`
- **Per wave merge:** Build clean + load in OBS + visual verification
- **Phase gate:** All five behaviors verified in OBS before `/gsd:verify-work`

### Wave 0 Gaps
None — no test infrastructure needed. Build verification is the gate.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection: `src/chalk-source.cpp`, `src/marks/cone-mark.cpp`, `src/marks/arrow-mark.cpp`, `src/marks/circle-mark.cpp`, `src/marks/freehand-mark.cpp`, `src/chalk-mode.cpp`, `src/tool-state.hpp`, `src/mark-list.hpp`
- OBS source settings pattern: verified from existing `obs_source_info` struct shape in chalk-source.cpp and OBS plugin template conventions
- `OBS_FRONTEND_EVENT_SCENE_CHANGED`: verified from existing frontend event callback pattern in plugin.cpp

### Secondary (MEDIUM confidence)
- Triangle strip cross-GPU reliability over gs_linewidth: based on known OBS rendering behavior and existing FreehandMark implementation pattern

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependencies; all APIs already in use
- Architecture: HIGH — all patterns derived from existing working code in the codebase
- Pitfalls: HIGH — root causes identified from direct code inspection; no speculation

**Research date:** 2026-04-08
**Valid until:** Indefinite — all findings from static codebase inspection; no external API versioning concern
