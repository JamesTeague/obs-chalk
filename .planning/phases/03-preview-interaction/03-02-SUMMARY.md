---
phase: 03-preview-interaction
plan: 02
subsystem: ui
tags: [qt6, tablet, pressure, freehand, tristrip, rendering]

requires:
  - phase: 03-preview-interaction-01
    provides: "chalk_input_begin/move dispatch, ChalkEventFilter with handle_tablet stub, QTabletEvent routing"

provides:
  - "FreehandPoint with pressure field (0.0-1.0)"
  - "FreehandMark::add_point(x, y, pressure) — pressure lerped across interpolated points"
  - "FreehandMark::draw() renders GS_TRISTRIP variable-width quad strip instead of GS_LINESTRIP"
  - "chalk_input_begin/move signatures updated with pressure param (default 1.0)"
  - "handle_tablet extracts QTabletEvent::pressure() and routes to dispatch"
  - "Ghost-stroke guard: TabletPress with pressure < 0.01 suppressed"

affects:
  - 03-preview-interaction-03
  - phase-4-distribution

tech-stack:
  added: []
  patterns:
    - "Variable-width stroke rendering: perpendicular offset per point, two vertices per centerline point emitted into GS_TRISTRIP"
    - "Pressure lerping: interpolated intermediate points lerp pressure between endpoints, not inherit from either"
    - "Default pressure=1.0f on mouse dispatch: all mouse callers use default param, no explicit pass required"

key-files:
  created: []
  modified:
    - src/marks/mark.hpp
    - src/marks/freehand-mark.hpp
    - src/marks/freehand-mark.cpp
    - src/chalk-source.hpp
    - src/chalk-source.cpp
    - src/chalk-mode.cpp

key-decisions:
  - "TRISTRIP rendering: for each centerline point, compute perpendicular direction to next (or previous at end) and emit two vertices offset by half-width. Degenerate segments fall back to previous direction."
  - "CHALK_MIN_WIDTH=1.5f / CHALK_MAX_WIDTH=6.0f: hairline at zero pressure to full chalk width at max — range chosen to feel like chalk on a chalkboard"
  - "Ghost-stroke threshold 0.01: Wacom tablets sometimes report TabletPress events with pressure=0.0 for hover proximity — threshold prevents invisible stroke artifacts"
  - "Pressure lerped on intermediate points: STEP=2px interpolation lerps pressure between previous and new point, giving smooth width transitions across high-speed strokes"

patterns-established:
  - "Pattern: chalk_input_begin/move use default parameter pressure=1.0f so all non-tablet callers are unaffected — no call sites needed updating"

requirements-completed: [INPT-03]

duration: 3min
completed: 2026-04-08
---

# Phase 3 Plan 02: Tablet Pressure Sensitivity Summary

**Freehand strokes rendered as pressure-variable width quad strips (GS_TRISTRIP) with QTabletEvent::pressure() routed through the input dispatch chain**

## Performance

- **Duration:** 3 min
- **Started:** 2026-04-08T19:20:58Z
- **Completed:** 2026-04-08T19:23:29Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Added pressure field to `FreehandPoint`; `add_point` signature updated with `pressure=1.0f` default — all existing call sites unaffected
- Replaced LINESTRIP rendering with a variable-width triangle strip: perpendicular offsets computed per centerline point, two vertices emitted per point into `GS_TRISTRIP`
- Interpolated intermediate points (STEP=2px loop) now lerp pressure between endpoints for smooth width transitions
- `handle_tablet` extracts `e->pressure()` and passes it to `chalk_input_begin/move`; `TabletPress` with pressure < 0.01 suppressed to prevent ghost strokes from hover events

## Task Commits

Each task was committed atomically:

1. **Task 1: Add pressure to input dispatch and FreehandMark data model** - `566423c` (feat)
2. **Task 2: Wire tablet pressure from event filter through dispatch** - `1e43f7b` (feat)

## Files Created/Modified
- `src/marks/mark.hpp` - `add_point` signature updated with `pressure=1.0f` default parameter
- `src/marks/freehand-mark.hpp` - `FreehandPoint` gains pressure field; `CHALK_MIN_WIDTH`/`CHALK_MAX_WIDTH` constants; `add_point` override with pressure
- `src/marks/freehand-mark.cpp` - `add_point` stores pressure, lerps on interpolated points; `draw()` uses GS_TRISTRIP variable-width rendering
- `src/chalk-source.hpp` - `chalk_input_begin/move` declarations updated with `pressure=1.0f` default
- `src/chalk-source.cpp` - Implementations updated; freehand branch passes pressure to `add_point`
- `src/chalk-mode.cpp` - `handle_tablet` extracts pressure, passes to dispatch; ghost-stroke guard; mouse handlers annotated

## Decisions Made

**TRISTRIP rendering approach:** For each centerline point, compute the direction to the next point (or previous at end) to get the along-stroke tangent, rotate 90 degrees for the perpendicular, then offset by half-width in both directions. This produces well-formed quads for variable-width strokes. Degenerate segments (zero-length) fall back to the previous direction or the x-axis, preventing NaN from the normalize step.

**Width constants (1.5f–6.0f):** Min 1.5px gives a visible hairline at near-zero pressure without disappearing. Max 6px matches thick chalk feel without overwhelming the video frame. These can be tuned per-tool if needed in a later phase.

**Ghost-stroke suppression threshold (0.01):** Wacom and similar tablets report `TabletProximity` followed by a `TabletPress` with pressure=0.0 when the pen enters hover range. Checking `pressure >= 0.01f` before starting a stroke prevents invisible zero-width marks from being added to the mark list.

**Pressure lerp on interpolated points:** When the STEP=2px interpolation inserts intermediate points, they lerp pressure from the previous point toward the incoming pressure. This avoids abrupt width jumps when strokes sample pressure at lower rates than movement.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None. Build succeeded clean on first attempt for both tasks.

## Next Phase Readiness
- Pressure-variable freehand strokes are functional; tablet users will see width variation immediately on next OBS load
- Plan 03-03 (verification checkpoint) can now test both mouse full-width and tablet pressure-variable rendering
- `distance_to` unchanged — still measures centerline distance, which is correct for pick-to-delete hit testing regardless of stroke width

---
*Phase: 03-preview-interaction*
*Completed: 2026-04-08*

## Self-Check: PASSED

All 6 modified source files verified present. Both task commits (566423c, 1e43f7b) confirmed in git log. Build verified: `** BUILD SUCCEEDED **` with zero errors and zero warnings.
