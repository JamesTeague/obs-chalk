# Phase 3: Preview Interaction - Research

**Researched:** 2026-04-06
**Domain:** Qt event filter on OBS preview widget, coordinate mapping, tablet pressure, global hotkeys
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
1. **Qt event filter on preview widget.** Not a transparent overlay window, not a dock panel. Event filter is the least invasive approach — single coordinate transform, no window management, no platform-specific overlay code. PRISM Studio validates the UX (drawing on preview). obs-draw validates the event filter pattern (plugin-level Qt event interception).
2. **Chalk mode is a global hotkey toggle.** Not source-scoped — chalk mode is a session-level state. Toggle on, draw, toggle off. This avoids requiring source selection to start drawing.
3. **Source interaction callbacks remain as fallback.** The existing `mouse_click` / `mouse_move` callbacks on `obs_source_info` stay. They work when a user clicks directly on the chalk source in interaction mode. The event filter is the primary path; source callbacks are the secondary path for users who prefer the traditional OBS interaction model.
4. **Transparent overlay window is the fallback approach.** If the Qt event filter hits fundamental limitations (e.g., OBS intercepts events before our filter, widget structure changes break `findChild`), we fall back to a transparent overlay window.
5. **Custom cursor when chalk mode active.** Visual feedback that the mode is on. Crosshair or dot — something that reads as "drawing tool" not "pointer."
6. **Pen/tablet pressure via QTabletEvent.** The event filter naturally receives `QTabletEvent` when a tablet pen is used. Pressure maps to stroke width.

### Claude's Discretion
- Widget lookup strategy: `findChild<QWidget*>("preview")` is the starting point, but the exact widget name/hierarchy may vary across OBS versions. Fallback strategies (walking the widget tree, checking class names) are implementation detail.
- Coordinate mapping implementation: whether to use OBS's own `GetMouseEventPos()` or reimplement the formula from available preview geometry.
- Cursor style: crosshair, dot, custom pixmap — whatever communicates chalk mode clearly.
- Pressure curve: linear vs. eased mapping from tablet pressure to stroke width.
- Stroke width range affected by pressure (min/max width bounds).
- Whether chalk mode state is persisted across OBS restarts or always starts off.
- Error handling when preview widget can't be found (graceful degradation to source-only interaction).

### Deferred Ideas (OUT OF SCOPE)
- Dock panel for tool selection (moved to Phase 5, optional)
- Dock panel showing active tool and color (moved to Phase 5, optional)
- Touch screen input support (v2)
- Eraser tip detection on tablet (post-v1, if demand validates)
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PREV-01 | Qt event filter on OBS preview widget intercepts mouse/pen events when chalk mode is active | Widget lookup confirmed: `findChild<OBSBasicPreview*>("preview")` on main window. Event filter pattern verified via obs-draw reference. |
| PREV-02 | Preview widget coordinates are mapped to scene space so marks render at the correct position | Full transform formula verified in OBSBasicPreview.cpp source + display-helpers.hpp. Plugin can compute independently using `GetScaleAndCenterPos`. |
| PREV-03 | Global OBS hotkey toggles chalk mode on/off; when off, all preview events pass through to normal OBS behavior | `obs_hotkey_register_frontend` confirmed in obs-hotkey.h. Requires ENABLE_FRONTEND_API and frontend event callback for widget lifecycle. |
| PREV-04 | Custom cursor indicates when chalk mode is active | Standard Qt cursor API: `widget->setCursor(Qt::CrossCursor)` / `widget->unsetCursor()`. No research blocker. |
| INPT-03 | Plugin accepts tablet/pen input with pressure sensitivity affecting stroke width | `QTabletEvent::pressure()` confirmed in Qt 6 headers. OBS does NOT consume tablet events in preview (no `tabletEvent` override) — plugin event filter receives `QEvent::TabletPress/Move/Release` natively. |
</phase_requirements>

---

## Summary

Phase 3 replaces the source-interaction-callback input path with a Qt event filter installed directly on the `OBSBasicPreview` widget. This is a well-trodden pattern in the OBS plugin ecosystem (obs-draw uses it). The implementation has three parts: (1) widget acquisition after OBS finishes loading, (2) coordinate mapping from Qt widget pixels to OBS scene pixels, and (3) per-event dispatch to existing mark creation logic.

The coordinate mapping is the most detail-intensive piece. `OBSBasicPreview::GetMouseEventPos()` is a private static method and cannot be called from a plugin. However, the underlying formula is fully reconstructable from widget geometry and `obs_get_video_info()`. The formula is already exposed as `GetScaleAndCenterPos()` in `display-helpers.hpp` (a public OBS utility), and `PREVIEW_EDGE_SIZE = 10` is confirmed in `OBSBasic.hpp`. obs-draw uses the same utility and the same approach — HIGH confidence.

Tablet pressure requires handling `QEvent::TabletPress/Move/Release` in the event filter. OBS's preview widget does NOT override `tabletEvent()` (verified by source inspection), meaning Qt would synthesize mouse events from pen input by default. Intercepting at the event filter level before Qt's synthesis path requires returning `true` from the filter handler, which prevents synthesis and gives chalk full control over pressure.

**Primary recommendation:** Install event filter on `OBSBasicPreview` via `OBS_FRONTEND_EVENT_FINISHED_LOADING` callback. Map coordinates using reimplemented `GetScaleAndCenterPos` + `PREVIEW_EDGE_SIZE`. Handle `QTabletEvent` first in the filter; fall through to `QMouseEvent` for non-tablet input. Route to existing mark creation logic unchanged.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt6::Widgets | 6.8.x (must match OBS.app) | QWidget event filter, cursor, QTabletEvent | OBS is built against Qt 6; plugin must link same version |
| Qt6::Core | 6.8.x | QObject, QEvent type system | Required for event filter base class |
| obs-frontend-api | 31.1.1 | `obs_hotkey_register_frontend`, `obs_frontend_get_main_window`, `obs_frontend_add_event_callback` | Only way to register global (non-source) hotkeys and get main QMainWindow* |
| libobs | 31.1.1 | `obs_get_video_info`, existing mark creation | Already linked |

### CMake changes required
```cmake
option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)  # was OFF
option(ENABLE_QT "Use Qt functionality" ON)  # was OFF
```

Both options already exist in `CMakeLists.txt`. Flipping them to ON is the only build change needed.

**Installation:** No new packages — OBS.app bundles Qt 6 and obs-frontend-api. CMakeLists already knows where to find them.

---

## Architecture Patterns

### Recommended File Structure
```
src/
├── plugin.cpp           # Add: frontend event callback, global state init/teardown
├── chalk-mode.hpp       # NEW: ChalkMode singleton — global hotkey + event filter lifecycle
├── chalk-mode.cpp       # NEW: event filter install/remove, coordinate mapping, event dispatch
├── chalk-source.hpp     # Existing — add chalk_mode_active flag or query ChalkMode
├── chalk-source.cpp     # Existing — mouse_click/move callbacks remain as fallback
├── mark-list.hpp        # Unchanged
├── mark-list.cpp        # Unchanged
├── tool-state.hpp       # Unchanged
└── marks/               # Unchanged
```

### Pattern 1: Deferred Widget Acquisition via Frontend Event

The preview widget does not exist at `obs_module_load` time — OBS creates it after all plugins load. Use `OBS_FRONTEND_EVENT_FINISHED_LOADING` to defer installation.

```cpp
// Source: OBSBasic.cpp:1298 — OBS fires FINISHED_LOADING after full UI init
// Source: decklink-ui-main.cpp:435 — confirmed plugin pattern

// In obs_module_load():
obs_frontend_add_event_callback(on_obs_event, nullptr);

static void on_obs_event(obs_frontend_event event, void *) {
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        chalk_mode_install_filter();
    }
    if (event == OBS_FRONTEND_EVENT_EXIT) {
        chalk_mode_remove_filter();
    }
}
```

### Pattern 2: Widget Lookup

The preview widget is named `"preview"` in `OBSBasic.ui` (line 212, confirmed). It is typed as `OBSBasicPreview`, which inherits `OBSQTDisplay`, which inherits `QWidget`.

From a plugin, you cannot include `OBSBasicPreview.hpp` (it is a frontend-internal header). Look up by name on the main window:

```cpp
// Source: OBSBasic.ui line 212 — widget name is "preview"
// obs_frontend_get_main_window() returns void* — cast to QMainWindow*
QMainWindow *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
QWidget *preview = main->findChild<QWidget *>("preview");
// preview is non-null if OBS is fully loaded; nullptr if widget renamed in future OBS version
```

Do NOT type-match on `OBSBasicPreview` — that header is not part of the plugin API.

### Pattern 3: Coordinate Mapping

`OBSBasicPreview::GetMouseEventPos()` is private. Reconstruct the equivalent from first principles.

```cpp
// Source: display-helpers.hpp GetScaleAndCenterPos (public OBS utility, MIT-compatible license)
// Source: OBSBasic_Preview.cpp ResizePreview — defines PREVIEW_EDGE_SIZE = 10

static vec2 widget_to_scene(QWidget *preview, float wx, float wy) {
    float dpr        = static_cast<float>(preview->devicePixelRatioF());
    QSize pixelSize  = preview->size() * preview->devicePixelRatioF();  // physical pixels

    obs_video_info ovi = {};
    obs_get_video_info(&ovi);

    constexpr int EDGE = 10;  // PREVIEW_EDGE_SIZE from OBSBasic.hpp
    int inner_w = pixelSize.width()  - EDGE * 2;
    int inner_h = pixelSize.height() - EDGE * 2;

    int   off_x;
    int   off_y;
    float scale;
    // GetScaleAndCenterPos computes letterbox offset and scale
    GetScaleAndCenterPos(
        static_cast<int>(ovi.base_width),
        static_cast<int>(ovi.base_height),
        inner_w, inner_h,
        off_x, off_y, scale);

    // Add edge, convert event pos (logical px) to physical px, then to scene px
    float phys_x = wx * dpr;
    float phys_y = wy * dpr;
    float scene_x = (phys_x - (off_x + EDGE)) / scale;
    float scene_y = (phys_y - (off_y + EDGE)) / scale;

    vec2 result;
    vec2_set(&result, scene_x, scene_y);
    return result;
}
```

This formula matches `GetMouseEventPos` in `OBSBasicPreview.cpp` lines 51-55. `pixelRatio / previewScale` becomes `dpr / scale`, and `previewX / pixelRatio` becomes `(off_x + EDGE) / dpr`. Confirmed equivalent after accounting for the `EDGE` offset applied in `ResizePreview`.

**Note on fixed scaling / scrolling:** The formula above handles the default "Scale to window" mode. In fixed-scaling mode, `GetCenterPosFromFixedScale` is used instead, and scrolling offsets are added. This is a discretion call — start with auto-scale. Fixed-scale with scroll adds ~10 lines.

### Pattern 4: Event Filter Class

```cpp
// Standard QObject event filter — no custom base class needed
class ChalkEventFilter : public QObject {
    Q_OBJECT
public:
    explicit ChalkEventFilter(ChalkSource *ctx, QObject *parent = nullptr)
        : QObject(parent), ctx_(ctx) {}

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (!chalk_mode_active_)
            return false;  // pass through

        switch (event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletMove:
        case QEvent::TabletRelease:
            return handle_tablet(static_cast<QTabletEvent *>(event));
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
            return handle_mouse(static_cast<QMouseEvent *>(event));
        default:
            return false;
        }
    }
private:
    ChalkSource *ctx_;
};
```

Install: `preview->installEventFilter(filter)` after widget lookup.
Remove: `preview->removeEventFilter(filter)` in `obs_module_unload`.

### Pattern 5: QTabletEvent Pressure Handling

```cpp
// Source: Qt 6 qevent.h line 355 — QTabletEvent::pressure() returns qreal 0.0-1.0
// OBS preview does NOT override tabletEvent() — verified by searching OBSBasicPreview.cpp
// Qt will synthesize mouse events from unhandled tablet events by default.
// Returning true from the filter prevents synthesis.

bool handle_tablet(QTabletEvent *e) {
    float pressure = static_cast<float>(e->pressure());
    float wx = static_cast<float>(e->position().x());  // Qt 6: use position(), not pos()
    float wy = static_cast<float>(e->position().y());
    vec2 scene = widget_to_scene(preview_, wx, wy);

    bool pen_down = (e->type() == QEvent::TabletPress || e->type() == QEvent::TabletMove);

    // Route to mark logic with pressure — pressure = 0.0 means pen lifted
    dispatch_input(ctx_, scene.x, scene.y, pen_down, pressure);
    return true;  // consume — prevent Qt mouse synthesis
}
```

### Pattern 6: Global Hotkey Registration

```cpp
// Source: obs-hotkey.h line 145 — obs_hotkey_register_frontend
// Must be called from Qt main thread (obs_module_load runs there)
// Global hotkeys appear in OBS Settings > Hotkeys under "Chalk" section

static obs_hotkey_id s_chalk_mode_hotkey = OBS_INVALID_HOTKEY_ID;
static bool s_chalk_mode_active = false;

static void chalk_mode_hotkey_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
    if (!pressed) return;
    s_chalk_mode_active = !s_chalk_mode_active;
    // Update cursor on preview widget
    if (s_preview_widget) {
        if (s_chalk_mode_active)
            s_preview_widget->setCursor(Qt::CrossCursor);
        else
            s_preview_widget->unsetCursor();
    }
}

// In obs_module_load():
s_chalk_mode_hotkey = obs_hotkey_register_frontend(
    "chalk.chalk_mode",
    "Chalk: Toggle Chalk Mode",
    chalk_mode_hotkey_cb,
    nullptr);
```

### Anti-Patterns to Avoid
- **Including OBSBasicPreview.hpp or OBSBasic.hpp in the plugin:** These are frontend-internal headers, not part of the plugin API. They will cause link failures against the installed OBS binary and break when OBS changes its internals.
- **Calling OBSBasicPreview::GetMouseEventPos() directly:** Private static method. Not accessible. Reimplement the formula instead.
- **Using `event->pos()` for QTabletEvent in Qt 6:** Deprecated. Use `event->position()`.
- **Registering chalk mode as a source hotkey:** Source hotkeys require source selection and are unbound when the source isn't in any scene. Global hotkeys via `obs_hotkey_register_frontend` are session-level.
- **Installing event filter before `OBS_FRONTEND_EVENT_FINISHED_LOADING`:** The preview widget is created lazily during OBS initialization. Installing during `obs_module_load` will find a null widget.
- **Not returning `true` for tablet events:** If the filter returns `false`, Qt synthesizes a mouse event from the tablet event, then the filter sees the synthetic mouse event too — double dispatch.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Letterbox/scale math | Custom aspect-ratio centering | `GetScaleAndCenterPos` from display-helpers.hpp | Already handles all edge cases (portrait vs landscape, square canvas); matches OBS exactly |
| Widget discovery | Custom widget tree walker | `QMainWindow::findChild<QWidget*>("preview")` | Direct name lookup is O(1) and stable across Qt versions; tree walking is fragile |
| Tablet pressure detection | Platform HID polling, custom driver | `QTabletEvent::pressure()` | Qt handles Wacom, Apple Pencil, generic stylus via platform plugins — no driver code needed |
| Global hotkey binding | Platform keylogger, CGEvent tap (macOS) | `obs_hotkey_register_frontend` | OBS hotkey system handles conflicts, serialization, user rebinding, all platforms |

**Key insight:** The coordinate mapping looks custom but is actually a reimplementation of a known formula. The formula is deterministic from widget size + video info. Do not try to read `OBSBasic::previewScale` directly (private field, not in plugin API).

---

## Common Pitfalls

### Pitfall 1: Widget Not Found at Plugin Load Time
**What goes wrong:** `findChild<QWidget*>("preview")` returns null when called from `obs_module_load`.
**Why it happens:** OBS creates the main window and preview widget after calling `obs_module_load` on all plugins. The widget simply doesn't exist yet.
**How to avoid:** Defer widget lookup to `OBS_FRONTEND_EVENT_FINISHED_LOADING` callback.
**Warning signs:** Null pointer crash in `installEventFilter` immediately on plugin load.

### Pitfall 2: Coordinate Off-by-PREVIEW_EDGE_SIZE * Scale
**What goes wrong:** Marks draw with a consistent ~10px offset at 1x scale, more offset at larger scales.
**Why it happens:** `ResizePreview` adds `PREVIEW_EDGE_SIZE = 10` (physical pixels) to both `previewX` and `previewY` after the letterbox calculation. If the mapping formula omits this, all coordinates are shifted by 10/scale scene units.
**How to avoid:** Include `EDGE = 10` offset in the coordinate formula as shown in Pattern 3.
**Warning signs:** Marks appear correct when canvas is small, increasingly wrong as preview is zoomed in.

### Pitfall 3: Double Dispatch from Tablet Synthesized Mouse Events
**What goes wrong:** Each pen stroke fires twice — once as `QTabletEvent`, once as synthesized `QMouseEvent`.
**Why it happens:** Qt synthesizes a mouse event for every unhandled tablet event (controlled by `Qt::AA_SynthesizeMouseForUnhandledTabletEvents`). If the filter returns `false` for tablet events, Qt then generates a mouse event and the filter sees it again.
**How to avoid:** Return `true` (consume) from the tablet event handler. If chalk mode handles tablet events, they must not fall through to mouse handling.
**Warning signs:** Every pen move creates two points in freehand strokes.

### Pitfall 4: Chalk Mode Active During OBS Hotkey Assignment
**What goes wrong:** User tries to bind a hotkey in OBS Settings but chalk mode is on, so all clicks in the preview are consumed by the event filter.
**Why it happens:** Event filter is indiscriminate — it intercepts all mouse events on the preview widget whenever chalk mode is active.
**How to avoid:** Check `obs_hotkey_get_registrations_count` or listen for a "hotkey binding" state — or simply document that chalk mode must be off to configure hotkeys. The simpler fix: toggling chalk mode off via hotkey always works even when the filter is active.
**Warning signs:** User reports unable to interact with preview while chalk mode is on (expected behavior, but needs documentation).

### Pitfall 5: Pressure Reads 0.0 for Non-Tablet Mouse Input
**What goes wrong:** Mouse strokes have zero-width because pressure defaults to 0.0 for mouse events.
**Why it happens:** `QMouseEvent` has no pressure. If the freehand tool always maps pressure to stroke width, mouse users get invisible strokes.
**How to avoid:** Use pressure=1.0 (full width) for mouse events; pressure from `QTabletEvent` only.
**Warning signs:** Freehand strokes invisible or hairline-thin when using a mouse.

### Pitfall 6: `obs_hotkey_register_frontend` Called Before Frontend API Initializes
**What goes wrong:** Hotkey is registered but never appears in OBS Settings > Hotkeys.
**Why it happens:** `obs_hotkey_register_frontend` requires the frontend to be initialized, which happens after `obs_module_load` returns for all plugins. However, `obs_module_load` itself is safe for hotkey registration — OBS queues frontend hotkeys during load and shows them once Settings opens.
**How to avoid:** Register in `obs_module_load` as normal. Do NOT defer hotkey registration to `FINISHED_LOADING` — that event fires too late for hotkeys to be loaded from the scene collection file.
**Warning signs:** Hotkey appears in Settings but any previously saved binding is lost on restart.

---

## Code Examples

### Complete Coordinate Mapping (verified against OBSBasicPreview.cpp)
```cpp
// Source: OBSBasic_Preview.cpp ResizePreview() + display-helpers.hpp GetScaleAndCenterPos()
// Source: OBSBasic.hpp line 89 — #define PREVIEW_EDGE_SIZE 10

#include <graphics/vec2.h>
#include <obs.h>
#include <QWidget>

static constexpr int CHALK_PREVIEW_EDGE = 10;

static vec2 preview_widget_to_scene(QWidget *preview, float event_x, float event_y) {
    float dpr = static_cast<float>(preview->devicePixelRatioF());

    // Physical pixel size of preview widget
    int phys_w = static_cast<int>(preview->width()  * dpr);
    int phys_h = static_cast<int>(preview->height() * dpr);

    // OBS canvas dimensions
    obs_video_info ovi = {};
    obs_get_video_info(&ovi);

    // Compute letterbox offset and scale (same as OBS ResizePreview)
    int   off_x, off_y;
    float scale;
    {
        int inner_w = phys_w - CHALK_PREVIEW_EDGE * 2;
        int inner_h = phys_h - CHALK_PREVIEW_EDGE * 2;
        // Inline GetScaleAndCenterPos logic (cannot include display-helpers.hpp from plugin)
        double wAspect = static_cast<double>(inner_w) / inner_h;
        double bAspect = static_cast<double>(ovi.base_width) / ovi.base_height;
        if (wAspect > bAspect) {
            scale  = static_cast<float>(inner_h) / ovi.base_height;
        } else {
            scale  = static_cast<float>(inner_w) / ovi.base_width;
        }
        int scaled_w = static_cast<int>(ovi.base_width  * scale);
        int scaled_h = static_cast<int>(ovi.base_height * scale);
        off_x = (inner_w - scaled_w) / 2 + CHALK_PREVIEW_EDGE;
        off_y = (inner_h - scaled_h) / 2 + CHALK_PREVIEW_EDGE;
    }

    // event_x/y are in logical pixels; convert to physical then to scene
    float scene_x = (event_x * dpr - off_x) / scale;
    float scene_y = (event_y * dpr - off_y) / scale;

    vec2 result;
    vec2_set(&result, scene_x, scene_y);
    return result;
}
```

Note: `display-helpers.hpp` cannot be included from a plugin — it pulls in internal OBS headers. The inline reimplementation above is 10 lines and matches exactly.

### Event Filter Registration (lifecycle)
```cpp
// In obs_module_load:
obs_frontend_add_event_callback([](obs_frontend_event event, void *) {
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
        chalk_mode_install();
    if (event == OBS_FRONTEND_EVENT_EXIT)
        chalk_mode_shutdown();
}, nullptr);

// chalk_mode_install():
static QWidget *s_preview = nullptr;
static ChalkEventFilter *s_filter = nullptr;

void chalk_mode_install() {
    QMainWindow *main = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (!main) return;
    s_preview = main->findChild<QWidget*>("preview");
    if (!s_preview) {
        blog(LOG_WARNING, "obs-chalk: could not find preview widget — event filter not installed");
        return;
    }
    s_filter = new ChalkEventFilter(/* ctx */);
    s_preview->installEventFilter(s_filter);
}
```

### Cursor Toggle
```cpp
// Qt built-in cursors — no pixmap needed for v1
// Source: Qt 6 Qt::CursorShape enum
void chalk_set_cursor(bool chalk_active) {
    if (!s_preview) return;
    if (chalk_active)
        s_preview->setCursor(Qt::CrossCursor);  // + crosshair — universal "drawing" symbol
    else
        s_preview->unsetCursor();  // restore OBS default
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `event->pos()` (QTabletEvent) | `event->position()` | Qt 6.0 | `pos()` still compiles but is deprecated and may be removed |
| Source-scoped hotkeys | `obs_hotkey_register_frontend` for session-level actions | OBS 28+ | Frontend hotkeys appear in Settings independent of source presence |
| Custom coordinate math | `GetScaleAndCenterPos` utility | OBS 28+ | Utility is stable; reimplementing the formula is preferred over including internal headers |

**Deprecated/outdated:**
- `QTabletEvent::pos()`: Returns `QPoint` integer. Qt 6 prefers `QTabletEvent::position()` returning `QPointF` for sub-pixel accuracy.
- OBS source interaction callbacks as primary drawing path: Works, but requires source selection and interaction mode. Event filter is the telestrator-grade path.

---

## Open Questions

1. **Fixed scaling / scroll offset accuracy**
   - What we know: In auto-scale mode (default), the formula in Pattern 3 is exact. In fixed-scaling mode, `GetCenterPosFromFixedScale` replaces `GetScaleAndCenterPos`, and scroll offsets are added.
   - What's unclear: Whether any OBS users commonly use fixed scaling during active streaming (most coaches use full-window auto-scale).
   - Recommendation: Implement auto-scale only for v1. Fixed-scale support is a post-launch issue to fix if a user reports it. Add a `// TODO: fixed-scale mode` comment in the mapping function.

2. **Widget name stability across OBS versions**
   - What we know: `"preview"` is set in `OBSBasic.ui` and has been stable through OBS 28-32 (confirmed in 31.1.1 source).
   - What's unclear: Whether OBS will rename it in a future version.
   - Recommendation: The lookup is one line. Wrap in a helper that logs a warning if null. Add a fallback: walk `QMainWindow` children looking for a widget whose class name contains "Preview".

3. **Thread for mark creation dispatch**
   - What we know: Qt event filter runs on Qt main thread. Source callbacks (`chalk_mouse_click`) are dispatched from `OBSBasicInteraction` — also Qt main thread (confirmed: `OBSBasicInteraction::HandleMouseClickEvent` calls `obs_source_send_mouse_click` inline). `mark_list.mutex` already protects render-thread / UI-thread races.
   - What's unclear: Nothing material. Event filter and source callbacks are both Qt main thread. No additional synchronization needed.
   - Recommendation: Document thread context in `chalk-mode.cpp` header comment. No code changes.

---

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None detected — C++ plugin with no test runner configured |
| Config file | None — see Wave 0 |
| Quick run command | `cmake --build .build/macos -- -j4 2>&1 \| grep -E "error:\|warning:"` (build-clean validation) |
| Full suite command | `cmake --build .build/macos && cp .build/macos/obs-chalk.plugin/Contents/MacOS/obs-chalk /Applications/OBS.app/Contents/PlugIns/obs-chalk.plugin/Contents/MacOS/obs-chalk` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PREV-01 | Event filter installed; chalk mode intercepts mouse press | manual-only | Build + OBS load + click preview | N/A |
| PREV-02 | Marks render at correct position relative to canvas | manual-only | Draw at corner of canvas; verify alignment | N/A |
| PREV-03 | Hotkey toggles chalk mode; pass-through works when off | manual-only | Bind hotkey; toggle; verify OBS source selection works | N/A |
| PREV-04 | Cursor changes to crosshair when chalk mode on | manual-only | Visual inspection | N/A |
| INPT-03 | Tablet pressure varies stroke width | manual-only | Draw with tablet; vary pressure; inspect stroke width | N/A |

**Why manual-only:** All requirements require OBS runtime (preview widget, display system, hotkey dispatch). The plugin is a `.plugin` bundle — there is no standalone test harness that can host OBS's Qt UI, display system, and plugin infrastructure.

### Sampling Rate
- **Per task commit:** Build succeeds (`cmake --build`) with no new warnings
- **Per wave merge:** Full install + OBS load, no crash on startup
- **Phase gate:** All 5 requirements manually verified before `/gsd:verify-work`

### Wave 0 Gaps
None — no test infrastructure is expected for this plugin type. Build-clean validation is the automated gate. Manual OBS verification is the phase gate.

---

## Sources

### Primary (HIGH confidence)
- OBSBasicPreview.hpp (`.deps/obs-studio-31.1.1/frontend/widgets/`) — widget class, GetMouseEventPos signature
- OBSBasicPreview.cpp — GetMouseEventPos implementation, coordinate formula (lines 47-58)
- OBSBasic_Preview.cpp — ResizePreview, PREVIEW_EDGE_SIZE usage (lines 203-234)
- OBSBasic.hpp — `#define PREVIEW_EDGE_SIZE 10` (line 89), previewX/Y/Scale field types
- display-helpers.hpp — GetScaleAndCenterPos inline implementation (lines 27-48)
- OBSBasic.ui — widget name `"preview"` confirmed (line 212)
- obs-hotkey.h — `obs_hotkey_register_frontend` signature (line 145)
- obs-frontend-api.h — `obs_frontend_get_main_window`, `obs_frontend_add_event_callback`
- Qt 6 qevent.h (bundled) — `QTabletEvent::pressure()`, `QTabletEvent::position()` (lines 319-355)
- obs-source.c — mouse callback dispatch on Qt main thread (lines 992-1012)

### Secondary (MEDIUM confidence)
- obs-draw/draw-dock.cpp (GitHub fetch) — confirmed: uses GetScaleAndCenterPos for coordinate mapping, QTabletEvent::pressure() for pressure, installEventFilter pattern. Structural match to our approach.
- OBSBasicInteraction.cpp — source callbacks confirmed on Qt main thread (line 299)
- decklink-ui-main.cpp — confirmed `OBS_FRONTEND_EVENT_FINISHED_LOADING` pattern for deferred init (line 435)

### Tertiary (LOW confidence)
- None — all critical claims verified from primary sources.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — Qt6 + obs-frontend-api confirmed in existing CMakeLists + OBS.app bundle
- Architecture (event filter pattern): HIGH — verified in OBS source + obs-draw reference
- Coordinate mapping formula: HIGH — verified directly in OBSBasicPreview.cpp and display-helpers.hpp
- Tablet pressure: HIGH — Qt 6 header confirmed; OBS non-interception confirmed by source search
- Thread safety: HIGH — both paths confirmed on Qt main thread via source trace

**Research date:** 2026-04-06
**Valid until:** 2026-05-06 (stable; OBS 31.x API changes slowly)
