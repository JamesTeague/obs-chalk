---
phase: 02-drawing-tools-mark-lifecycle-and-hotkeys
plan: "03"
subsystem: ui
tags: [obs, hotkeys, telestrator, drawing-tools, laser-pointer, pick-to-delete]

# Dependency graph
requires:
  - phase: 02-drawing-tools-mark-lifecycle-and-hotkeys/02-01
    provides: interfaces — ToolState, MarkList, ChalkSource with hotkey ID fields, mutex
  - phase: 02-drawing-tools-mark-lifecycle-and-hotkeys/02-02
    provides: ArrowMark, CircleMark, ConeMark with draw()/update_end()/distance_to()
provides:
  - "9 OBS hotkeys registered under source: undo, clear, next-color, laser, freehand, arrow, circle, cone, pick-to-delete"
  - "Tool routing in chalk_mouse_click dispatching on active_tool for all 5 tool types"
  - "Drag preview in chalk_mouse_move via update_end() for arrow/circle/cone"
  - "Laser pointer rendering in chalk_video_render (radius-8 circle, mutex-protected position)"
  - "Pick-to-delete mode with 20px threshold and auto-exit after first deletion"
  - "Color cycling across 5-color CHALK_PALETTE wrapping on CHALK_PALETTE_SIZE"
affects:
  - phase-03-ui-refinement
  - phase-04-release

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Hotkey callbacks as static C functions: fire on key-down only (except laser which also fires key-up)"
    - "Laser position under mutex: video_render reads laser_x/laser_y from a different thread"
    - "draw() and update_end() separation: marks are mutable while drawing completes on mouse-up via commit_mark()"

key-files:
  created: []
  modified:
    - src/chalk-source.cpp

key-decisions:
  - "Laser is independent of active_tool — laser_active flag renders dot overlay regardless of selected tool"
  - "pick_delete_mode auto-exits after single deletion — avoids user needing to press hotkey again"
  - "delete_closest threshold set to 20px — generous enough for click accuracy, specific enough for dense marks"
  - "chalk-source.cpp held to 373 lines via clean static-callback pattern — well under 500-line limit"

patterns-established:
  - "Static hotkey callbacks receive ChalkSource* via void* data — single pattern for all 9 hotkeys"
  - "All gs_* calls stay inside chalk_video_render — no exceptions added in this plan"
  - "Mouse callbacks serialized by OBS — no mutex needed for drawing/pick_delete_mode flags"

requirements-completed: [TOOL-05, MARK-01, MARK-02, MARK-03, MARK-04, INPT-01, INPT-02]

# Metrics
duration: ~2 sessions
completed: 2026-03-30
---

# Phase 02 Plan 03: Hotkeys, Tool Routing, and Laser Integration Summary

**9 OBS hotkeys wiring all 5 drawing tools, undo/clear/color-cycle, laser pointer, and pick-to-delete into chalk-source.cpp, verified working in OBS**

## Performance

- **Duration:** ~2 sessions
- **Started:** 2026-03-30T18:00:00Z
- **Completed:** 2026-03-30T20:04:00Z
- **Tasks:** 2 (1 auto + 1 human-verify)
- **Files modified:** 1 (src/chalk-source.cpp)

## Accomplishments

- Registered 9 hotkeys appearing in OBS Settings > Hotkeys under the Chalk source name
- Tool routing in chalk_mouse_click dispatches on active_tool across Freehand, Arrow, Circle, Cone, Laser
- Drag preview for all shape tools via update_end() in chalk_mouse_move
- Laser pointer renders as colored dot (8px radius, 16-segment circle) while key held, disappears on release, leaves no mark
- Pick-to-delete enters mode on hotkey press, deletes closest mark within 20px on click, auto-exits
- Color cycle steps through white, yellow, red, blue, green and wraps
- chalk-source.cpp: 373 lines (under 500-line limit)
- All features verified functional in OBS by user

## Task Commits

1. **Task 1: Hotkeys, tool routing, laser, pick-to-delete, and color cycle** - `91e7949` (feat)
2. **Task 2: Verify all Phase 2 features in OBS** - human-verify checkpoint, approved by user

Additional commit (out of plan scope, separate fix):
- `0593a00` - chore: dev-install.sh script to avoid stale binary issue from Xcode incremental build

## Files Created/Modified

- `src/chalk-source.cpp` - Rewritten with 9 hotkey callbacks, full tool routing, laser rendering, pick-to-delete, drag preview

## Decisions Made

- Laser is independent of active_tool. Setting laser_active=true renders the dot regardless of which tool is selected — laser is an overlay mode, not a mutually exclusive tool.
- pick_delete_mode auto-exits after the first deletion. This matches the expected UX: press hotkey, click a mark, done. Staying in mode would be surprising.
- delete_closest uses a 20px threshold. Wide enough for practical accuracy, narrow enough to avoid deleting unintended marks in dense drawings.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

**Stale binary problem (build system, not plan scope):** Xcode generator did not reliably detect source changes on incremental builds, causing OBS to load an old version of the plugin. Fixed by adding `scripts/dev-install.sh` which always reconfigures cmake before building. This was committed separately (`0593a00`) as it is not part of the feature work.

**UX and mark aesthetics deferred:** User noted that while all features are functional, the UX and visual quality of marks (e.g., arrow aesthetics, cone rendering, mark thickness) need refinement. These are not Phase 2 blockers and are deferred to a future phase (likely Phase 3 UI refinement).

## Deferred Items

- Arrow arrowhead aesthetics
- Cone filled triangle visual quality
- Mark thickness / stroke width controls
- General UX polish for drawing interaction

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Phase 2 is complete. All 11 requirements across plans 01-02-03 are verified in OBS.

Readiness for Phase 3 (UI refinement):
- All 5 tools work functionally — Phase 3 can focus purely on aesthetics without structural changes
- ToolState/MarkList/ChalkSource interfaces are stable — adding UI parameters (stroke width, opacity) is additive
- The stale-binary build issue is resolved — dev-install.sh should be used going forward

Concerns:
- Phase 4 macOS codesigning remains a deferred blocker (noted in STATE.md)

---
*Phase: 02-drawing-tools-mark-lifecycle-and-hotkeys*
*Completed: 2026-03-30*
