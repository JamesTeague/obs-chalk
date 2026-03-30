# Phase 2: Drawing Tools, Mark Lifecycle, and Hotkeys - Context

**Gathered:** 2026-03-30
**Status:** Ready for planning
**Source:** Discussion in session 83

<domain>
## Phase Boundary

Phase 2 adds all drawing tools, mark management (undo, delete, clear), OBS hotkeys, and color cycling on top of the Phase 1 scaffold. Phase 1 proved the object model pipeline (mouse -> mark list -> video_render). Phase 2 makes it a usable telestrator.

Existing code: src/plugin.cpp, src/chalk-source.{hpp,cpp}, src/tool-state.hpp, src/marks/mark.hpp, src/marks/freehand-mark.{hpp,cpp}, src/mark-list.{hpp,cpp}

</domain>

<decisions>
## Implementation Decisions

### Color Palette (INPT-02)
- 5 colors: white, yellow, red, blue, green
- Hotkey-driven cycle that wraps (green -> back to white)
- No color picker dialog — preset palette only

### Cone of Vision (TOOL-04)
- Rendered as a filled semi-transparent isosceles triangle
- Interaction: click sets apex (player's helmet), drag sets one base edge (defines direction, length, AND width simultaneously), other base edge is auto-mirrored across the apex-to-midpoint axis
- One click-drag gesture, symmetric result
- Starting angle ~60 degrees as a reference, but the actual angle is fully determined by where the user drags — no hardcoded angle

### Pick-to-Delete (MARK-02)
- Distance-from-stroke hit testing — closest mark to the click point wins
- Not bounding box (freehand strokes overlap too much for bounding boxes to be useful)

### Freehand Improvement (TOOL-01)
- Add linear interpolation between consecutive mouse points to fill gaps during fast movement
- Phase 1 proved gaps exist with raw mouse events

### Claude's Discretion
- Arrow end style variations (types and visual design)
- Circle rendering approach (outline vs filled, line width)
- Undo stack depth
- Specific hotkey defaults for each action
- Laser pointer visual style (cursor dot size, color, opacity)
- Hit-test distance threshold for pick-to-delete
- How pick-to-delete mode is entered/exited (hotkey toggle, or automatic after one deletion)

</decisions>

<specifics>
## Specific Ideas

- Cone: isosceles triangle geometry. Apex = click point. Axis = line from apex to midpoint of base. One base corner = drag endpoint. Other base corner = reflection of drag endpoint across the axis.
- Colors are football-standard — visible against typical game film backgrounds
- Wrapping cycle means the user can always get to any color with at most 4 presses

</specifics>

<deferred>
## Deferred Ideas

- Post-draw mark editing (select mark, drag handles to adjust) — applies to ALL mark types, not just cones. PowerPoint-style interaction. Deferred to post-v1.
- Tablet pressure sensitivity (Phase 3)
- Qt dock panel (Phase 3)

</deferred>

---

*Phase: 02-drawing-tools-mark-lifecycle-and-hotkeys*
*Context gathered: 2026-03-30 via discussion*
