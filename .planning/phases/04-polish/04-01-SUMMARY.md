---
phase: 04-polish
plan: 01
subsystem: rendering
tags: [obs, graphics, GS_TRISTRIP, cpp, marks]

# Dependency graph
requires:
  - phase: 03-preview-interaction
    provides: FreehandMark GS_TRISTRIP pattern (reference implementation)
provides:
  - Arrow shaft and arrowhead arms rendered as 6px GS_TRISTRIP
  - Circle outline rendered as 6px GS_TRISTRIP
  - Cone side edges rendered as 6px GS_TRISTRIP alongside existing GS_TRIS fill
affects: [04-polish, 05-release]

# Tech tracking
tech-stack:
  added: []
  patterns: [draw_thick_segment static helper for 4-vertex GS_TRISTRIP per segment]

key-files:
  created: []
  modified:
    - src/marks/arrow-mark.cpp
    - src/marks/circle-mark.cpp
    - src/marks/cone-mark.cpp
    - src/chalk-source.cpp

key-decisions:
  - "draw_thick_segment static helper at file scope in arrow-mark.cpp and cone-mark.cpp to avoid repeating 4-vertex tristrip boilerplate three times per file"
  - "Circle radial normal (cos_a, sin_a) serves directly as the perpendicular offset direction — no extra computation needed"
  - "Cone base edge (corner1 to corner2) deferred per CONTEXT.md — only two side edges get tristrip treatment in this plan"

patterns-established:
  - "draw_thick_segment(x1, y1, x2, y2, half_w): file-scope static helper emitting 4-vertex GS_TRISTRIP quad for any line segment"
  - "HALF_W = 3.0f constant (6px total) matches freehand CHALK_MAX_WIDTH = 6.0f at full pressure"

requirements-completed: [UX-01]

# Metrics
duration: 12min
completed: 2026-04-08
---

# Phase 4 Plan 01: Mark Visual Weight Polish Summary

**All non-freehand marks (arrow, circle, cone edges) converted from 1px GS_LINESTRIP to 6px GS_TRISTRIP, matching freehand stroke visual weight**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-04-08T21:30:00Z
- **Completed:** 2026-04-08T21:42:00Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- ArrowMark shaft and both arrowhead arms render at 6px via `draw_thick_segment` helper
- CircleMark outline rendered as 130-vertex GS_TRISTRIP using radial normals (65 pairs)
- ConeMark two side edges rendered as 6px GS_TRISTRIP; GS_TRIS fill unchanged
- Zero GS_LINESTRIP calls remain in any mark file

## Task Commits

Each task was committed atomically:

1. **Task 1: Convert ArrowMark draw() to GS_TRISTRIP at 6px** - `f5a48c2` (feat)
2. **Task 2: Convert CircleMark and ConeMark draw() to GS_TRISTRIP at 6px** - `db7f06d` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified
- `src/marks/arrow-mark.cpp` - Replaced 3x GS_LINESTRIP with draw_thick_segment(); added static helper
- `src/marks/circle-mark.cpp` - Replaced GS_LINESTRIP circle loop with 2-vertex-per-step GS_TRISTRIP using radial normals
- `src/marks/cone-mark.cpp` - Added draw_thick_segment static helper; appended two side-edge tristrips after GS_TRIS fill
- `src/chalk-source.cpp` - Fixed pre-existing hotkey_laser → hotkey_tool_laser rename mismatch (Rule 1 auto-fix)

## Decisions Made
- `draw_thick_segment` as file-scope static in each compilation unit (rather than shared utility) — both files are independent .cpp units, no shared header needed; keeps the pattern self-contained
- Circle uses the radial direction directly as the perpendicular offset — this is mathematically exact since the tangent to a circle is perpendicular to the radius
- Cone base edge deferred per CONTEXT.md — only two side edges treated here

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed hotkey_laser → hotkey_tool_laser rename mismatch in chalk-source.cpp**
- **Found during:** Task 1 (build verification after arrow-mark.cpp changes)
- **Issue:** chalk-source.hpp had renamed `hotkey_laser` to `hotkey_tool_laser` in an uncommitted change, but chalk-source.cpp still referenced the old name at two call sites (hotkey registration and unregistration), breaking the x86_64 build
- **Fix:** Renamed both `ctx->hotkey_laser` references to `ctx->hotkey_tool_laser` in chalk-source.cpp
- **Files modified:** src/chalk-source.cpp
- **Verification:** Build succeeded for both arm64 and x86_64 targets
- **Committed in:** f5a48c2 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - bug)
**Impact on plan:** Pre-existing rename mismatch that blocked the build. Fix was a two-line rename, no scope creep.

## Issues Encountered
- Build directory named `build_macos` not `build/macos` as the plan's verify command assumed — used correct path throughout.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All three mark types now visually consistent with freehand strokes at 6px
- Phase 04 plan 02 (if any) can build on consistent visual weight baseline
- ConeMark base edge tristrip deferred — flag for any future polish pass

---
*Phase: 04-polish*
*Completed: 2026-04-08*
