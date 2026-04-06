# CONTEXT: Phase 3 — Preview Interaction

## What This Phase Does

Moves drawing input from OBS source interaction callbacks to a Qt event filter on the OBS preview widget. Users draw directly on the preview with a hotkey-toggled chalk mode. This is the phase that makes obs-chalk feel like a real telestrator — no source selection, no interaction mode, just toggle chalk and draw.

## Current State

Phases 1-2 shipped: object model, all 5 tools, mark lifecycle, hotkeys. Input currently flows through `obs_source_info.mouse_click` / `obs_source_info.mouse_move` — requires clicking on the chalk source in the preview, which means the source must be selected and in interaction mode. This works for verification but isn't how a coach uses a telestrator during a livestream.

## Architecture

### Input Pathway Change

**Before (Phases 1-2):** User clicks OBS preview -> OBS routes to source interaction callback -> `chalk_mouse_click` / `chalk_mouse_move` on `ChalkSource`

**After (Phase 3):** User presses chalk mode hotkey -> Qt event filter installed on preview widget intercepts mouse/pen events -> coordinates mapped from preview widget space to scene space -> marks created on `ChalkSource` -> OBS renders marks via existing `video_render`

The rendering pipeline is unchanged. Only the input pathway changes.

### How the Event Filter Works

1. On plugin load, locate OBS preview widget via `findChild<QWidget*>("preview")` on the main OBS window
2. Install a `QObject` event filter on that widget
3. When chalk mode is active and a mouse/pen event arrives:
   - Map coordinates from preview widget space to scene space
   - Route to mark creation (same logic currently in `chalk_mouse_click` / `chalk_mouse_move`)
   - Consume the event (return `true`) so OBS doesn't process it
4. When chalk mode is inactive:
   - Pass events through (return `false`) — normal OBS select/move/resize behavior

### Coordinate Mapping

Preview widget displays the scene scaled and possibly letterboxed. The mapping formula (from OBS source):

```
scene_x = (widget_x - previewX) / previewScale
scene_y = (widget_y - previewY) / previewScale
```

Where `previewX`, `previewY`, `previewScale` account for the preview's offset and scaling within the widget. `devicePixelRatio` applies on HiDPI displays.

### Chalk Mode

A global OBS hotkey (not source-scoped) toggles chalk mode on/off. When on:
- Event filter intercepts mouse/pen input on preview
- Custom cursor indicates chalk mode is active
- All existing tool/color/undo/clear hotkeys work as before
When off:
- Event filter passes through
- Normal OBS preview behavior (source selection, transform handles, etc.)

## Decisions

1. **Qt event filter on preview widget.** Not a transparent overlay window, not a dock panel. Event filter is the least invasive approach — single coordinate transform, no window management, no platform-specific overlay code. PRISM Studio validates the UX (drawing on preview). obs-draw validates the event filter pattern (plugin-level Qt event interception).
2. **Chalk mode is a global hotkey toggle.** Not source-scoped — chalk mode is a session-level state. Toggle on, draw, toggle off. This avoids requiring source selection to start drawing.
3. **Source interaction callbacks remain as fallback.** The existing `mouse_click` / `mouse_move` callbacks on `obs_source_info` stay. They work when a user clicks directly on the chalk source in interaction mode. The event filter is the primary path; source callbacks are the secondary path for users who prefer the traditional OBS interaction model.
4. **Transparent overlay window is the fallback approach.** If the Qt event filter hits fundamental limitations (e.g., OBS intercepts events before our filter, widget structure changes break `findChild`), we fall back to a transparent overlay window. This is well-proven (gInk, Epic Pen) but more complex (platform-specific window code, position tracking, dual coordinate transforms).
5. **Custom cursor when chalk mode active.** Visual feedback that the mode is on. Crosshair or dot — something that reads as "drawing tool" not "pointer."
6. **Pen/tablet pressure via QTabletEvent.** The event filter naturally receives `QTabletEvent` when a tablet pen is used. Pressure maps to stroke width.

## Discretion

- Widget lookup strategy: `findChild<QWidget*>("preview")` is the starting point, but the exact widget name/hierarchy may vary across OBS versions. Fallback strategies (walking the widget tree, checking class names) are implementation detail.
- Coordinate mapping implementation: whether to use OBS's own `GetMouseEventPos()` or reimplement the formula from available preview geometry.
- Cursor style: crosshair, dot, custom pixmap — whatever communicates chalk mode clearly.
- Pressure curve: linear vs. eased mapping from tablet pressure to stroke width.
- Stroke width range affected by pressure (min/max width bounds).
- Whether chalk mode state is persisted across OBS restarts or always starts off.
- Error handling when preview widget can't be found (graceful degradation to source-only interaction).

## Deferred

- Dock panel for tool selection (moved to Phase 5, optional)
- Dock panel showing active tool and color (moved to Phase 5, optional)
- Touch screen input support (v2)
- Eraser tip detection on tablet (post-v1, if demand validates)

## Design Map

Items that may need revisiting during execution:

- [ ] **Widget lifecycle**: When does the preview widget get created relative to plugin load? May need deferred installation (timer or signal-based).
- [ ] **Multi-preview**: OBS can have multiple preview widgets (studio mode has two). Does the event filter need to handle both?
- [ ] **Thread safety**: Event filter runs on Qt main thread. Mark creation currently happens on OBS's UI thread via source callbacks. Need to verify these are the same thread, or add synchronization.
- [ ] **Source interaction coexistence**: When chalk mode is off and user interacts with the chalk source directly (source callbacks), does everything still work correctly? No regressions from event filter installation.
- [ ] **macOS accessibility**: Qt event filters on other applications' widgets may require accessibility permissions on macOS. Need to test — obs-chalk is loaded in-process so this likely isn't an issue.

## References

- OBS preview widget: `OBSBasicPreview` class, `obs_display_add_draw_callback` for display hooks
- PRISM Studio: github.com/naver/prism-live-studio (GPL-2.0) — draws on preview by modifying `window-basic-preview.cpp`. Proves UX, can't fork.
- obs-draw: github.com/exeldro/obs-draw — dock panel with `OBSQTDisplay`. Proven Qt event filter pattern. Tablet pressure via custom proc handler.
- gInk: github.com/geovens/gInk — transparent overlay approach (fallback reference)
