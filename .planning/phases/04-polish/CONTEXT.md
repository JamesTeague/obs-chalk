# Phase 4: Polish — Discussion Context

## Decisions

### Line width: all marks render at 6px to match freehand-with-mouse
- Freehand with mouse defaults pressure=1.0, giving 6.0px total width (3.0px half-width)
- Arrow, circle, and cone edges currently use default OBS line width (~1px)
- All non-freehand marks must render at 6px to match
- Freehand constants: CHALK_MIN_WIDTH=1.5, CHALK_MAX_WIDTH=6.0 in freehand-mark.hpp

### Cone opacity: fix alpha passthrough, then evaluate base edge
- Root cause identified: palette colors have alpha 0xFF (1.0). ConeMark constructor default of 0.35f is never reached because palette alpha is passed explicitly.
- Fix: pass 0.35f (or tuned value) instead of palette alpha when creating cones
- The blend state, shader, and pipeline are all correct — pure data issue
- Base edge: evaluate after opacity fix. If the fill is transparent enough, the base edge may blend away naturally. Don't engineer a removal solution until we see it with working alpha.

### Delete mode: persistent toggle
- Currently auto-exits after one deletion (pick_delete_mode = false immediately after delete_closest)
- Change: stay in delete mode until hotkey toggles it back off
- User clicks multiple marks to delete them one at a time, then toggles back to draw mode

### Laser pointer: selectable tool, not hold-hotkey
- Currently: hold hotkey to show, release to hide. Independent of chalk mode.
- Change: laser becomes a selectable tool in the tool set (via hotkey cycle or toolbar)
- Shows on mouse-down, disappears on mouse-up
- Requires chalk mode to be active (same as other tools)
- Visual stays the same: filled circle, 8px radius, current color, full opacity

### Scene-transition clear (INTG-01)
- Existing requirement, no changes from original scope

## AI Discretion

- Exact cone alpha value (start with 0.35, tune if needed)
- Whether line width for non-freehand marks uses triangle strip rendering or gs line width API (prefer whatever gives consistent cross-GPU results)
- How laser tool integrates into ToolState/ToolType enum and tool cycling

## Deferred

- Base edge removal on cone: evaluate after opacity fix, only pursue if still visually problematic
- Full clickable toolbar: v1.1
- Stream deck integration: future, needs research
