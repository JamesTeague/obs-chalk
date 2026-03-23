# Architecture Research

**Domain:** Native OBS Studio drawing overlay plugin (C/C++)
**Researched:** 2026-03-23
**Confidence:** MEDIUM — OBS plugin threading guarantees are underdocumented; core patterns are HIGH confidence from official docs, thread safety recommendations are MEDIUM from community practice.

## Standard Architecture

### System Overview

```
┌──────────────────────────────────────────────────────────────┐
│                     OBS Studio Process                        │
│                                                              │
│  ┌─────────────────────┐    ┌──────────────────────────────┐ │
│  │     UI Thread        │    │       Render Thread          │ │
│  │                      │    │                              │ │
│  │  ┌───────────────┐  │    │  ┌──────────────────────┐   │ │
│  │  │ ChalkDockPanel │  │    │  │  ChalkSource          │   │ │
│  │  │  (QDockWidget) │  │    │  │  video_render()       │   │ │
│  │  │               │  │    │  │  mouse_click()        │   │ │
│  │  │  tool buttons │  │    │  │  mouse_move()         │   │ │
│  │  │  color picker │  │    │  │                       │   │ │
│  │  │  clear button │  │    │  │  reads: MarkList      │   │ │
│  │  └───────┬───────┘  │    │  └──────────┬───────────┘   │ │
│  │          │           │    │             │                 │ │
│  │  ┌───────▼───────┐  │    │  ┌──────────▼───────────┐   │ │
│  │  │  ToolState    │◄─┼────┼─►│  MarkList (mutex)    │   │ │
│  │  │  (active tool │  │    │  │  vector<Mark>         │   │ │
│  │  │   color, etc) │  │    │  │  in-progress mark     │   │ │
│  │  └───────────────┘  │    │  └──────────────────────┘   │ │
│  │                      │    │                              │ │
│  │  Hotkey callbacks    │    │  gs_draw primitives per Mark │ │
│  │  Scene transition    │    │  (lines, beziers, shapes)    │ │
│  │  event callbacks     │    │                              │ │
│  └─────────────────────┘    └──────────────────────────────┘ │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              Plugin Entry Point (plugin.cpp)          │   │
│  │  obs_module_load() — registers source, hooks frontend │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|------------------------|
| `plugin.cpp` | Module entry point: registers source type, adds dock to OBS, registers frontend event callbacks | `obs_module_load()`, `obs_register_source()`, `obs_frontend_add_dock_by_id()` |
| `ChalkSource` | OBS source object: owns the render loop, receives mouse/key input from preview, drives mark creation | `obs_source_info` struct with callbacks; one instance per scene item |
| `MarkList` | Shared mutable state: ordered list of committed marks, in-progress mark, mutex protection | `std::vector<std::unique_ptr<Mark>>` + `std::mutex` |
| `Mark` (abstract base) | A single drawable object with a `draw()` method | Pure virtual base; concrete subclasses per tool type |
| `ToolState` | Active tool, active color, any per-tool config (arrow style, etc.) | Plain struct; written by UI thread, read by render thread via atomics or mutex |
| `ChalkDockPanel` | Qt dock panel: tool selection, color swatches, clear button | `QDockWidget` subclass, signals to `ChalkSource` via `obs_source_update()` or direct method call under lock |
| `HotkeyManager` | Registers and handles all OBS hotkeys (tool switch, undo, clear, laser toggle, color cycle) | Calls into `ChalkSource` or `MarkList` from hotkey callback; must be thread-safe |

---

## Recommended Project Structure

```
src/
├── plugin.cpp              # obs_module_load/unload, source registration, dock setup
├── chalk-source.hpp        # ChalkSource class declaration
├── chalk-source.cpp        # obs_source_info callbacks, input routing, render dispatch
├── mark-list.hpp           # MarkList class with mutex
├── mark-list.cpp           # add/undo/clear/pick-delete operations
├── marks/
│   ├── mark.hpp            # Abstract Mark base class: draw(), hit_test(), bounding_box()
│   ├── freehand-mark.hpp   # Polyline / bezier freehand path
│   ├── freehand-mark.cpp
│   ├── arrow-mark.hpp      # Start + end point, end-cap style
│   ├── arrow-mark.cpp
│   ├── circle-mark.hpp     # Center + radius
│   ├── circle-mark.cpp
│   ├── cone-mark.hpp       # Apex + direction vector + spread angle
│   ├── cone-mark.cpp
│   └── laser-mark.hpp      # Ephemeral; not added to MarkList, rendered live
├── tool-state.hpp          # Active tool enum, active color, tool config
├── hotkey-manager.hpp
├── hotkey-manager.cpp      # obs_hotkey_register_source calls, callback dispatch
├── dock/
│   ├── chalk-dock.hpp      # QDockWidget subclass declaration
│   └── chalk-dock.cpp      # Qt UI: buttons, color swatches, layout
└── CMakeLists.txt
```

### Structure Rationale

- **`marks/`:** Each mark type is a self-contained unit. The abstract base means the render loop never switches on type — it calls `mark->draw()`. New tools add files here without touching the render loop.
- **`dock/`:** Qt UI code stays isolated. Nothing in the libobs layer should `#include` Qt headers — this prevents the Qt/libobs header pollution problem.
- **`plugin.cpp` is thin:** Module load/unload only. No logic lives here.
- **`mark-list.*` separate from `chalk-source.*`:** The list is the shared state boundary. Keeping it separate makes the mutex contract explicit and testable.

---

## Architectural Patterns

### Pattern 1: Object Polymorphism for Marks

**What:** Each mark type is a concrete subclass of an abstract `Mark` base. The render loop iterates the list and calls `mark->draw(gs_effect_t *effect)` on each. The render loop has zero knowledge of mark types.

**When to use:** Always — this is the core departure from obs-draw's pixel buffer approach and the reason selective deletion is trivial.

**Trade-offs:** Minor vtable dispatch overhead per mark per frame. At ~10 marks and 60fps this is immeasurable. Worth it unconditionally.

**Example:**
```cpp
// mark.hpp
class Mark {
public:
    virtual ~Mark() = default;
    virtual void draw(gs_effect_t *solid) const = 0;
    virtual bool hit_test(float x, float y) const = 0;
};

// render loop in chalk-source.cpp
void chalk_source_render(void *data, gs_effect_t *effect) {
    auto *ctx = static_cast<ChalkSource *>(data);
    std::lock_guard<std::mutex> lock(ctx->mark_list.mutex);
    for (const auto &mark : ctx->mark_list.marks) {
        mark->draw(effect);
    }
    if (ctx->mark_list.in_progress) {
        ctx->mark_list.in_progress->draw(effect);
    }
}
```

### Pattern 2: In-Progress Mark Split from Committed List

**What:** While the user is actively drawing, the mark-being-created lives in a separate `in_progress` slot outside the committed `marks` vector. On mouse-up, it moves into `marks`. On escape or cancel, it's discarded.

**When to use:** Always. This keeps the render loop simple (render committed + in_progress), undo trivial (pop `marks.back()`), and cancel clean (nullptr the in_progress slot).

**Trade-offs:** Minimal. One extra null-check per frame.

### Pattern 3: Mutex-Guarded Shared State

**What:** `MarkList` owns a `std::mutex`. Every access — from the render thread (read), mouse callbacks (write), hotkey callbacks (write), or dock UI (write) — takes this lock.

**When to use:** Everywhere the mark list is touched. OBS threading guarantees between UI/hotkey/render callbacks are not formally documented; treat all of them as potentially concurrent.

**Trade-offs:** Lock contention could theoretically cause a render frame to wait. In practice, mark list operations are microseconds; contention is not a concern at this scale.

**Example:**
```cpp
void MarkList::undo_last() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!marks.empty()) {
        marks.pop_back();
    }
}
```

### Pattern 4: Laser Pointer as Non-Committed Mark

**What:** The laser pointer never enters `marks`. It renders only while the hold-key is active and renders from the current cursor position stored atomically. No allocation, no list mutation, no undo entry.

**When to use:** Specifically for the laser tool. Generalizes to any "ephemeral indicator" that should not persist.

**Trade-offs:** Requires one extra render path. Worth it — treating laser as a committed mark would pollute undo history.

---

## Data Flow

### Drawing Flow (Mouse-Driven)

```
User draws in OBS preview
    |
    v
OBS routes mouse_move / mouse_click to ChalkSource callbacks
(runs on render thread)
    |
    v
ChalkSource checks ToolState.active_tool
    |
    +-- if press: create in_progress Mark of correct subtype, store cursor pos
    |
    +-- if drag:  append point / update geometry on in_progress mark
    |
    +-- if release: lock MarkList, move in_progress -> marks, unlock
    |
    v
Next video_render() call: lock MarkList, iterate marks + in_progress, call draw(), unlock
    |
    v
OBS composites the source output onto the scene
```

### Hotkey Flow

```
User presses hotkey (e.g., Undo)
    |
    v
OBS calls registered hotkey callback
(thread: unspecified — treat as any thread)
    |
    v
HotkeyManager dispatches to MarkList::undo_last()
    |
    v
MarkList acquires mutex, pops last mark, releases mutex
    |
    v
Next video_render() call reflects updated state
```

### Tool/Color Change Flow

```
User clicks tool button in ChalkDockPanel (Qt UI thread)
    |
    v
Qt slot updates ToolState (atomic write or under mutex)
    |
    v
ChalkSource reads ToolState.active_tool on next mouse_press
(no immediate render effect — ToolState is config, not state)
```

### Scene Transition Flow (optional clear-on-transition)

```
OBS emits OBS_FRONTEND_EVENT_SCENE_CHANGED
    |
    v
plugin.cpp frontend event callback fires (UI thread)
    |
    v
If clear-on-transition is enabled in source settings:
    MarkList::clear_all() under mutex
    |
    v
Next video_render() call renders empty list
```

---

## Component Boundaries

| Boundary | Communication | Direction | Notes |
|----------|---------------|-----------|-------|
| `ChalkSource` <-> `MarkList` | Direct method calls under mutex | Both directions | ChalkSource is the only writer; render loop is reader. Lock on every access. |
| `ChalkDockPanel` <-> `ToolState` | Direct struct write | Dock writes, Source reads | ToolState is config; use `std::atomic` for simple fields (enum, color index) to avoid full mutex for reads |
| `HotkeyManager` <-> `MarkList` | Direct method calls | HotkeyManager writes | Hotkey callbacks may fire from any thread; always lock |
| `plugin.cpp` <-> `ChalkSource` | `obs_source_create()` / `obs_source_destroy()` | OBS lifecycle | Plugin module holds no direct pointer to source — OBS manages lifetime |
| `ChalkSource` <-> `ChalkDockPanel` | `obs_frontend_add_event_callback` + direct pointer (init-time) | Source notifies dock of cursor position for laser feedback | Pass dock pointer to source at construction; source calls dock update methods on UI thread via Qt signals |
| `Mark` subclasses <-> graphics layer | `gs_*` API calls inside `draw()` | Mark -> OBS graphics | `draw()` must only be called from within the OBS graphics context (render thread or after `obs_enter_graphics()`) |

---

## Suggested Build Order

Dependencies flow upward; build lower layers first.

```
1. marks/mark.hpp (abstract base, no dependencies)
       |
2. marks/{freehand,arrow,circle,cone}-mark.{hpp,cpp}
   (implement draw() using gs_* — can stub gs_* for unit testing)
       |
3. mark-list.{hpp,cpp}
   (depends on Mark base, owns the mutex)
       |
4. tool-state.hpp
   (plain struct — no deps, can be in same PR as mark-list)
       |
5. chalk-source.{hpp,cpp}
   (depends on MarkList, ToolState; wires up obs_source_info callbacks)
       |
6. hotkey-manager.{hpp,cpp}
   (depends on MarkList, ChalkSource — registers hotkeys in source create)
       |
7. dock/chalk-dock.{hpp,cpp}
   (depends on ToolState — Qt UI, no libobs deps except obs_frontend_api.h)
       |
8. plugin.cpp
   (ties everything together: registers source, adds dock, wires events)
```

**Rationale:** The Mark type hierarchy and MarkList are the kernel of the plugin — all render and input logic depends on them being solid. The dock and hotkeys are additive after the core render loop works. This order lets you verify drawing in the preview before adding any Qt UI.

---

## Anti-Patterns

### Anti-Pattern 1: Pixel Buffer / Texture Accumulation

**What people do:** Render marks into a persistent `gs_texrender_t` / `gs_texture_t` each frame, accumulating paint strokes into GPU memory.

**Why it's wrong:** This is the obs-draw architecture. GPU texture resources acquired on the render thread can race with OBS's own resource management on scene transitions and source destroy. The result is the crash obs-chalk exists to fix. Marks cannot be individually deleted from a pixel buffer without re-rendering everything, which reintroduces the race window on every clear operation.

**Do this instead:** Vector object model. Each mark stores its geometry. Each frame redraws all marks using gs_draw primitives (lines, triangles). GPU state is transient — no accumulated texture to race over.

### Anti-Pattern 2: Calling Qt UI from Render Thread

**What people do:** Update Qt widgets directly from `video_render()` or mouse callbacks (which run on the render thread).

**Why it's wrong:** Qt UI must only be touched from the main (UI) thread. Calling Qt methods from the render thread causes non-deterministic crashes. This is the most common OBS plugin crash class after the texture race.

**Do this instead:** Post updates to the Qt thread via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`. For the laser pointer cursor position, write to an `std::atomic<float>` pair in the source; the dock panel reads it on a Qt timer tick.

### Anti-Pattern 3: Storing OBS Object Pointers Across Callbacks

**What people do:** Cache `obs_source_t*` or `obs_scene_t*` pointers in plugin state without incrementing the reference count.

**Why it's wrong:** OBS uses reference counting. A raw pointer held without `obs_source_addref()` can dangle when the source is removed from the scene while the plugin still holds it.

**Do this instead:** Use `obs_source_get_ref()` / `obs_source_release()` pairs, or the RAII `OBSSource` wrapper from `obs.hpp`. For the dock needing access to the source, use `obs_get_source_by_name()` which returns a ref-counted handle.

### Anti-Pattern 4: Monolithic Source File

**What people do:** Put all mark types, the render loop, mouse handling, dock code, and hotkey registration in one `plugin.cpp` or `source.cpp`.

**Why it's wrong:** The 500-line file limit is load-bearing here — not just convention. Drawing tools accumulate quickly (5 mark types x ~80 lines each = 400 lines before the render loop). A monolith makes the mutex contract invisible and forces the render thread and Qt UI code to share a translation unit.

**Do this instead:** The project structure above. One file per mark type, MarkList isolated, dock isolated.

---

## Integration Points

### OBS Source System

| Hook | Purpose | Notes |
|------|---------|-------|
| `obs_source_info.video_render` | Render all marks each frame | Runs in graphics context; safe to call `gs_*` directly |
| `obs_source_info.mouse_click` | Begin/end mark creation | Requires `OBS_SOURCE_INTERACTION` flag |
| `obs_source_info.mouse_move` | Update in-progress mark geometry | Same flag |
| `obs_source_info.get_width/height` | Source dimensions for coordinate mapping | Must match the target video resolution |
| `obs_source_info.create/destroy` | Initialize/free MarkList, register hotkeys | `destroy` must release any `gs_*` resources within graphics context |
| `obs_source_info.save/load` | Persist hotkey bindings | Call `obs_hotkey_save/load` in these callbacks |

### OBS Frontend API

| Hook | Purpose | Notes |
|------|---------|-------|
| `obs_frontend_add_dock_by_id()` | Add ChalkDockPanel to OBS window | OBS 30+; call from `obs_module_load()` |
| `obs_frontend_add_event_callback()` | Listen for `OBS_FRONTEND_EVENT_SCENE_CHANGED` | Fires on UI thread; safe to update MarkList under mutex |

### OBS Hotkey API

| Function | Purpose | Notes |
|----------|---------|-------|
| `obs_hotkey_register_source()` | Register per-source hotkeys (tool switch, undo, clear) | Call in `create` callback; OBS persists bindings in scene collection file |
| `obs_hotkey_register_frontend()` | Register global hotkeys not tied to source lifetime | Use for laser toggle if it needs to work outside preview interaction |

---

## Scaling Considerations

This is a single-user desktop plugin. "Scaling" means source instance count, not users.

| Concern | Reality | Approach |
|---------|---------|---------|
| Multiple scenes | Each scene item = one ChalkSource instance | MarkList per instance; no shared state between instances |
| Mark count | Upper bound ~10 per play; 100 is extreme | No optimization needed; redrawing 100 vector marks at 60fps is microseconds |
| Freehand path points | Long strokes can accumulate thousands of points | Cap or simplify path on mouse-up using Ramer-Douglas-Peucker; prevents unbounded growth across a long session |
| Tablet input | Pressure data in `obs_mouse_event` | `obs_mouse_event` includes pressure; freehand mark can store width variation per point. Implement after core works. |

---

## Sources

- [OBS Source API Reference](https://docs.obsproject.com/reference-sources) — `obs_source_info` fields, interaction flags, lifecycle callbacks (HIGH confidence)
- [OBS Rendering Graphics](https://docs.obsproject.com/graphics) — graphics context rules, effects system (HIGH confidence)
- [OBS Frontend API Reference](https://docs.obsproject.com/reference-frontend-api) — `obs_frontend_add_dock_by_id`, event callbacks, hotkeys (HIGH confidence)
- [OBS Backend Design](https://docs.obsproject.com/backend-design) — threading model, pipeline architecture (HIGH confidence)
- [OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate) — canonical starting point for new plugins (HIGH confidence)
- [OBS Thread Safety Discussion](https://github.com/obsproject/obs-studio/discussions/5556) — explicit acknowledgment that thread safety guarantees are underdocumented (MEDIUM confidence — informs caution, not specific patterns)
- [OBS Frontend API Dock Discussion](https://obsproject.com/forum/threads/obs-add-a-new-custom-panel-to-the-dock.78945/) — `obs_frontend_add_dock_by_id` pattern and `QMainWindow` integration (MEDIUM confidence)

---

*Architecture research for: native OBS Studio drawing overlay plugin (obs-chalk)*
*Researched: 2026-03-23*
