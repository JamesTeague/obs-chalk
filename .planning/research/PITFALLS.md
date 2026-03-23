# Pitfalls Research

**Domain:** Native C/C++ OBS Studio plugin — drawing overlay with GPU rendering, Qt6 dock, OBS hotkeys
**Researched:** 2026-03-23
**Confidence:** HIGH (GPU threading, obs-draw crash analysis from source), MEDIUM (build system, macOS distribution), LOW (some Qt dock teardown specifics)

---

## Critical Pitfalls

### Pitfall 1: Calling Graphics Functions Outside the Graphics Context

**What goes wrong:**
Any call to libobs graphics functions (`gs_texrender_*`, `gs_effect_*`, `gs_draw_*`, etc.) outside the graphics context causes an immediate crash or undefined behavior. The graphics context is a mutex-protected resource owned by one thread at a time.

**Why it happens:**
Developers call these functions from proc_handlers, hotkey callbacks, frontend event callbacks, or Qt UI event handlers — all of which run on threads that do not hold the graphics context. obs-draw's crash is exactly this: `draw_clear()` and `copy_to_undo()` call `obs_enter_graphics()` from proc_handler callbacks (which are invoked from arbitrary threads, often the UI/main thread), then also attempt to call graphics functions again. If the graphics thread is already mid-render when the proc_handler fires, you have a reentrancy or deadlock.

**How to avoid:**
Never call graphics functions (`gs_*`) directly from hotkey callbacks, proc handlers, frontend event callbacks, or Qt slots. Instead, use a state flag or command queue: set a "pending_clear" boolean from any thread, then consume it in `video_render()` which is already on the graphics thread (or explicitly guarded by `obs_enter_graphics/leave_graphics` in the correct window). For obs-chalk's object model, this is natural — mouse/hotkey events write to the mark list (protected by a mutex or atomic), `video_render()` reads it. No GPU work happens outside `video_render`.

**Warning signs:**
- Calling `obs_enter_graphics()` from a hotkey callback, proc handler, or Qt slot
- A `draw_clear` or `undo` function that calls `obs_enter_graphics()` and is invoked by non-render callbacks
- Any `gs_*` call that is not inside `video_render()` or inside an explicit `obs_enter_graphics/leave_graphics` block
- OBS crash log showing "graphics thread" in the stack trace

**Phase to address:** Phase 1 (core rendering foundation). Architecture must be established before any GPU work.

---

### Pitfall 2: Nested or Reentrant Graphics Context Entry

**What goes wrong:**
Calling `obs_enter_graphics()` when the current thread already holds the graphics context (or when another thread holds it and you're blocking on it) causes a deadlock. This is documented in multiple OBS issues, including the CEF browser plugin deadlock and Lua script freeze.

**Why it happens:**
obs-draw has this exact pattern: `draw_clear()` calls `copy_to_undo()` first (which calls `obs_enter_graphics()/leave_graphics()`), then calls `obs_enter_graphics()` again. If `draw_clear` is called from a context that already holds the graphics lock (e.g., called from `video_render` by mistake), the second `obs_enter_graphics()` in `copy_to_undo` deadlocks. The documented fix for scripts is: never call `obs_enter_graphics()` from `source_info.create` or `source_info.destroy` — defer to `video_render` instead.

**How to avoid:**
Calls to `obs_enter_graphics/leave_graphics` must be single-level. Never nest them. Never call a function that internally calls `obs_enter_graphics` from within a block that already holds the graphics context. For obs-chalk: since `video_render` is already on the graphics thread, do not call `obs_enter_graphics/leave_graphics` inside it. All GPU work goes in `video_render`. If you need GPU operations during `create` (e.g., loading effects), a single top-level `obs_enter_graphics/leave_graphics` block is fine — but do not call any function that itself calls `obs_enter_graphics` inside it.

**Warning signs:**
- Any code path where `obs_enter_graphics()` is called inside a function that is itself called inside another `obs_enter_graphics` block
- OBS hangs (full freeze) rather than crashes — this is the hallmark of a deadlock, not a race
- Call chain: `video_render` → some function → `obs_enter_graphics()`

**Phase to address:** Phase 1 (rendering architecture). The rule must be established as an invariant before any GPU code is written.

---

### Pitfall 3: GPU Resource Allocation/Destruction Outside the Graphics Context

**What goes wrong:**
Creating or destroying GPU objects (`gs_texrender_t`, `gs_effect_t`, `gs_vertbuffer_t`, `gs_texture_t`) outside the graphics context causes a crash or silent memory corruption. GPU objects must be created inside `obs_enter_graphics()` and destroyed inside `obs_enter_graphics()`.

**Why it happens:**
Developers forget that GPU objects have GPU-side state. Allocating them without the graphics context means the GPU backend (Metal on macOS, D3D on Windows, OpenGL on Linux) may not have a valid device context for the allocation.

**How to avoid:**
- Create GPU resources in `ds_create` inside a single `obs_enter_graphics/leave_graphics` block (this is what obs-draw does correctly for effect creation)
- Destroy GPU resources in `ds_destroy` inside a single `obs_enter_graphics/leave_graphics` block
- For obs-chalk, the render list contains CPU-side geometry (vec2 arrays, color values). No GPU textures or vertex buffers need to be pre-allocated per mark. Render geometry on-the-fly in `video_render` using `gs_render_start/stop` or pre-built static effect + transform matrix. This dramatically reduces GPU resource lifecycle complexity.

**Warning signs:**
- `gs_texrender_create` or `gs_effect_create_from_file` called outside a graphics context block
- `gs_texrender_destroy` or `gs_effect_destroy` called in a non-guarded path
- Conditional `obs_enter_graphics` calls (the obs-draw `ds_destroy` pattern of "enter graphics only if needed" is fragile — prefer always-enter for cleanup)

**Phase to address:** Phase 1 (rendering architecture) and Phase 2 (each tool implementation).

---

### Pitfall 4: Modifying the Mark List Without Thread Protection

**What goes wrong:**
The render list is read every frame by `video_render` on the graphics thread and written by hotkey callbacks, mouse event handlers, and Qt dock actions on other threads. Without synchronization, you get use-after-free, torn reads, or rendering a partially-modified mark.

**Why it happens:**
The object model feels like it only has CPU-side state, so developers treat it like regular data and skip locking. But OBS's graphics thread and main/UI thread genuinely run concurrently.

**How to avoid:**
Protect the mark list with a `pthread_mutex_t` (libobs provides `pthread_mutex_t` and helpers). Lock it in `video_render` when iterating marks, and lock it in any callback that modifies the list (hotkey, mouse, Qt slot). Alternatively, use a lock-free command queue (ring buffer) to post mutations from producer threads to the graphics thread. For obs-chalk's small list (~10 marks), a simple mutex is correct and sufficient.

**Warning signs:**
- Mark list accessed in `video_render` without a lock
- Mouse event handlers or hotkey callbacks directly push/pop the mark list
- Intermittent crashes during rapid tool switching or undo

**Phase to address:** Phase 1 (data model design). Must be in place before any mark operations are implemented.

---

### Pitfall 5: Qt UI Operations Called from Non-Qt Threads

**What goes wrong:**
Qt requires all UI operations to run on the Qt main thread. Calling any Qt widget, signal, or slot from the graphics thread or audio thread causes undefined behavior — typically a crash in Qt internals with no clear stack trace.

**Why it happens:**
OBS callbacks like `video_render`, `video_tick`, hotkey callbacks, and proc handlers are called on various non-Qt threads. Developers who update dock UI state from these callbacks (e.g., updating a tool indicator label, enabling/disabling buttons) hit this immediately.

**How to avoid:**
Use `QMetaObject::invokeMethod(obj, "method", Qt::QueuedConnection, ...)` to post UI updates to the Qt event loop from any thread. Or use Qt signals/slots with `Qt::QueuedConnection` explicitly. Never directly call Qt widget methods from OBS rendering or callback threads.

**Warning signs:**
- Qt widget methods called from `video_tick`, `video_render`, or hotkey callbacks
- Crash stack trace containing `QObject`, `QWidget`, or Qt event loop internals
- Crash only occurs when the dock is visible/active

**Phase to address:** Phase 3 (Qt dock integration).

---

### Pitfall 6: OBSQTDisplay / Qt Display Destruction Race

**What goes wrong:**
If an `OBSQTDisplay` (or its underlying `obs_display_t`) is destroyed while the graphics thread is still trying to draw to it, OBS crashes with a context-switch error (documented in OBS PR #4576: GLXBadDrawable on Linux, similar patterns on macOS with Metal). The race window is small but real.

**Why it happens:**
Qt's `DeleteLater` defers object destruction by an indeterminate amount of time. During that window, the draw surface may be invalid but the display's draw callback is still registered. For obs-chalk, this matters if the dock's embedded preview display is destroyed during OBS shutdown or dock undocking.

**How to avoid:**
If obs-chalk embeds a preview display in the Qt dock (showing source output with drawing canvas), explicitly destroy the `obs_display_t` (by calling `obs_display_destroy`) when the Qt surface becomes invalid — do not rely on the destructor chain. The safer approach is to not embed a preview display at all: use a standalone dock for tool controls, and let the OBS preview window handle source display. This eliminates the entire class of problem.

**Warning signs:**
- Using `OBSQTDisplay` anywhere in the dock implementation
- OBS crash on dock close or OBS shutdown
- Stack trace involving `obs_display_t` destruction and graphics thread simultaneously

**Phase to address:** Phase 3 (Qt dock). Architectural decision: avoid embedding an OBSQTDisplay in the dock entirely.

---

### Pitfall 7: Wrong Source Type for Drawing Overlay

**What goes wrong:**
Implementing obs-chalk as an `OBS_SOURCE_TYPE_FILTER` (applied to another source) vs `OBS_SOURCE_TYPE_INPUT` (standalone source added to a scene) has significant behavioral differences. A filter cannot be independently positioned or layered — it processes its parent source's pixels. An input source is independently positioned and composited by OBS.

**Why it happens:**
Developers use filter because it's how they think about "drawing on top of something." But a filter in OBS wraps the source's render pipeline — it replaces the parent's video_render. This makes independent z-ordering and transparent overlay compositing complex.

**How to avoid:**
obs-chalk should be an `OBS_SOURCE_TYPE_INPUT` with `OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_CAP_OBSOLETE` (check current flags). The source renders a transparent RGBA overlay on top of whatever is below it in the scene. Users add it to their scene as a top-layer source. This is the correct architecture for a drawing overlay. Set `get_width/get_height` to return the output canvas size (match OBS base resolution), and render marks with alpha-compositing blend state.

**Warning signs:**
- Source type set to `OBS_SOURCE_TYPE_FILTER` for a drawing overlay
- Having to call `obs_source_process_filter_begin/end` in `video_render`
- Mark positions are relative to parent source rather than screen

**Phase to address:** Phase 1 (source registration and type selection).

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Pixel buffer / texture painting (obs-draw's approach) | Simple "draw to canvas" mental model | No selective delete, undo requires texture snapshots, clear requires GPU resource recreation that races with render thread | Never — object model is the entire point of obs-chalk |
| Rebuilding vertex data every frame via `gs_render_start/stop` for freehand strokes | Simple — just add points to a temp buffer each render | 45µs wasted per call on iGPU, prevents async GPU execution | Acceptable for MVP if mark count stays small (~10 marks, each with ~50 points). Profile before optimizing. |
| Skip mutex on mark list, rely on "it's fast enough" | Saves 5 lines of locking code | Intermittent crashes under load; impossible to reproduce in testing | Never |
| Call obs_enter_graphics from non-render callbacks | Allows GPU work from hotkey handlers | Direct cause of obs-draw's crash pattern; deadlock risk | Never — always defer GPU work to video_render |
| Not calling obs_hotkey_unregister in ds_destroy | One less function to remember | Memory leak of hotkey registrations; potential use-after-free on source reload | Never |
| Conditional obs_enter_graphics in destroy (obs-draw pattern) | Avoids unnecessary lock acquisition | Fragile — if code path analysis is wrong, destroys GPU object without context | Never — always use obs_enter_graphics unconditionally for GPU object cleanup |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| OBS hotkey system | Registering hotkeys in `obs_module_load` before the source is created | Register hotkeys in `ds_create` via `obs_hotkey_register_source`; they persist with the source, survive scene collection changes |
| OBS hotkey system | Not saving/loading hotkey bindings with source settings | Use `obs_hotkey_save` / `obs_hotkey_load` in `get_settings` / `update` callbacks |
| OBS frontend events | Calling frontend API in `obs_module_load` before OBS frontend is fully initialized | Wrap frontend calls that need to happen post-load in `obs_module_post_load`, not `obs_module_load` |
| `OBS_FRONTEND_EVENT_SCENE_CHANGED` | Assuming this fires at transition start | It fires after transition *completes*. For clear-on-transition, this means marks are visible during the transition — that is probably the correct behavior for obs-chalk |
| Qt dock | Calling `obs_frontend_add_dock_by_id` from a thread other than the main thread | Always call from `obs_module_load` which runs on the main thread |
| Qt dock | Creating dock widget without a parent | Always pass the OBS main window as parent: `static_cast<QMainWindow*>(obs_frontend_get_main_window())` |
| Mouse events in source | Expecting mouse_click/mouse_move to fire when OBS is in "preview" mode without interaction enabled | User must enable "Interact" mode in OBS for mouse events to reach the source; the dock-based design avoids this for tool control, but in-preview drawing requires it |

---

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Rebuilding freehand mark geometry every frame | CPU spike during active drawing; mild on desktop GPU, significant on iGPU (45µs per call documented) | Acceptable for MVP at ~10 marks with ~50 points each; cache vertex data if profiling shows it as hot | Never a real problem at the scale obs-chalk operates (~10 marks, not 1000) |
| Long-running work in video_render | Frame drops, encoding lag, "Frames missed due to rendering lag" in OBS stats | Keep video_render fast: iterate mark list, emit draw calls, exit. No file I/O, no allocation. | Any video_render that takes > 1ms at 60fps budget (16.7ms) risks frame drops |
| Allocating mark objects with `new` / `malloc` per mouse move event | Memory fragmentation; not a performance issue at obs-chalk scale | Use a flat array or pre-allocated pool for freehand point data | Not a real concern for ~10 marks with ~50 points each |
| Holding graphics context lock longer than necessary | Other OBS components stall waiting for graphics context | Enter, do GPU work, leave. Do not call non-GPU code inside obs_enter_graphics blocks. | Immediate — OBS video output stutters |

---

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Hotkeys that only work when OBS has focus | Coach can't draw while focus is on video playback software | OBS global hotkeys work system-wide by default — register via `obs_hotkey_register_source`, not Qt key events |
| Tool mode requires clicking in the OBS preview to draw | Coach must switch focus to OBS preview, disrupting workflow | Design the source so interaction works via the preview's Interact mode; make hotkeys the primary workflow so focus switching is rare |
| Clear-on-scene-transition ON by default | Marks disappear unexpectedly when switching cameras | Default to OFF; user opts in. Document clearly. |
| Laser pointer persists after hotkey release (if toggled vs hold) | Distracting red dot on stream | Toggle-hold is correct — hold to show, release to hide. No toggle state to forget. |
| Pick-to-delete mode requires precise clicking | Frustrating on small/overlapping marks | Use generous hit testing — bounding box + small tolerance, not pixel-perfect. Accept imprecision. |

---

## "Looks Done But Isn't" Checklist

- [ ] **Hotkeys:** Verify hotkeys survive source delete + re-add and OBS restart. Check that `obs_hotkey_save/load` are wired to the source settings callbacks.
- [ ] **Clear-on-scene-transition:** Verify that the frontend event callback is unregistered in `ds_destroy`. Failure = callback fires on a destroyed source.
- [ ] **Mark list mutex:** Verify that every path that reads or writes the mark list either holds the lock or is provably single-threaded. grep for all mark_list accesses.
- [ ] **GPU object cleanup:** Verify that `ds_destroy` enters the graphics context and destroys all `gs_*` objects. Missing cleanup causes crash on source delete or OBS exit.
- [ ] **Freehand mark completeness:** A freehand stroke in progress when OBS is closed should not crash. Ensure in-progress marks are either committed or discarded cleanly.
- [ ] **Laser pointer is truly ephemeral:** Verify laser pointer state is never added to the mark list and is reset on undo, clear, and source deactivation.
- [ ] **get_width/get_height callbacks:** Must be implemented and return the correct canvas size. Missing callbacks cause OBS to treat the source as 0x0, making it invisible.
- [ ] **OBS_SOURCE_CUSTOM_DRAW flag:** If not set, OBS may apply an optimization that conflicts with custom RGBA compositing. Verify alpha blending of marks over transparent background works correctly.

---

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| GPU resource race (like obs-draw) | HIGH | Architectural rewrite — pixel-buffer architecture cannot be safely patched. Object model prevents this entirely. |
| Deadlock from nested obs_enter_graphics | MEDIUM | Identify all call sites of obs_enter_graphics; audit for nesting. grep for double-entry patterns. Restructure to defer GPU work to video_render. |
| Qt crash from non-main-thread Qt calls | LOW-MEDIUM | grep for Qt widget method calls in OBS callbacks; wrap in QMetaObject::invokeMethod with Qt::QueuedConnection. |
| Hotkeys lost after OBS restart | LOW | Add obs_hotkey_save/load calls to get_settings/update. Verify in testing. |
| Marks visible during transition when clear-on-transition expected | LOW | Check event timing — OBS_FRONTEND_EVENT_SCENE_CHANGED fires post-transition. May be desired behavior. |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| GPU functions outside graphics context | Phase 1: Rendering Architecture | Code review rule: no `gs_*` calls outside `video_render` or explicit `obs_enter_graphics` block. grep enforces this. |
| Nested obs_enter_graphics deadlock | Phase 1: Rendering Architecture | Prove by inspection that obs_enter_graphics is never called inside video_render, and never nested. |
| GPU resource allocation without context | Phase 1: Source registration + create/destroy | Verify all `gs_texrender_create` / `gs_effect_create` calls are in graphics context blocks. |
| Mark list thread safety | Phase 1: Data model | Mutex created in ds_create, locked in video_render and all mutation paths. Verified by code review. |
| Qt UI from non-Qt thread | Phase 3: Qt dock integration | All dock update calls use Qt::QueuedConnection. Audit before dock phase ships. |
| OBSQTDisplay destruction race | Phase 3: Qt dock integration | Architectural decision: don't embed OBSQTDisplay in dock. Use tool-only dock panel. |
| Wrong source type | Phase 1: Source registration | `OBS_SOURCE_TYPE_INPUT` established in first commit, never changes. |
| Hotkey persistence | Phase 2: Hotkey integration | Test: delete source, re-add, verify hotkeys restore from scene collection. |
| Clear-on-transition callback leak | Phase 2: Transition integration | Code review: frontend event callback removed in ds_destroy. |
| macOS codesigning | Phase: Distribution | Use obs-plugintemplate CI/CD; configure bundleId and Developer ID certs before first public release. |

---

## Sources

- obs-draw source code analysis (`/Users/jteague/dev/obs-draw/draw-source.c`): direct observation of the GPU race condition in `draw_clear` / `copy_to_undo` / `apply_tool` — all call `obs_enter_graphics` from proc_handler callbacks
- [OBS Studio PR #4576: Fix crash on *nix caused by OBSQTDisplay destruction after draw surface invalidation](https://github.com/obsproject/obs-studio/pull/4576) — authoritative source for the DeleteLater race pattern
- [OBS GitHub Discussion #5556: Unclear thread safety requirements for plugins](https://github.com/obsproject/obs-studio/discussions/5556) — confirms thread safety documentation gap
- [OBS GitHub Issue: obs-browser deadlock from obs_enter_graphics in CEF thread](https://github.com/obsproject/obs-browser/issues/333) — deadlock pattern from calling obs_enter_graphics from wrong context
- [OBS GitHub Issue: Lua deadlocks when calling obs_enter_graphics from create/destroy](https://github.com/obsproject/obs-studio/issues/6674) — documented fix: defer to video_render
- [OBS Rendering Graphics documentation](https://docs.obsproject.com/graphics) — graphics context rules
- [OBS Source API Reference](https://docs.obsproject.com/reference-sources) — callback contracts, get_width/get_height requirements
- [OBS Frontend API Reference](https://docs.obsproject.com/reference-frontend-api) — OBS_FRONTEND_EVENT_SCENE_CHANGED timing
- [OBS GitHub Issue #2872: gs_draw_sprite rebuilds vertex buffer per frame](https://github.com/obsproject/obs-studio/issues/2872) — vertex buffer performance pitfall
- [DeepWiki: libobs Core](https://deepwiki.com/obsproject/obs-studio/2.1-libobs-core) — threading architecture, destruction_task_thread, reference counting
- [OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate) — macOS codesigning/notarization requirements, buildspec.json

---
*Pitfalls research for: Native C/C++ OBS Studio drawing overlay plugin (obs-chalk)*
*Researched: 2026-03-23*
