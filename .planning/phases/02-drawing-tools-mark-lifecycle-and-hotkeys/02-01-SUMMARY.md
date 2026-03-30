---
phase: 02-drawing-tools-mark-lifecycle-and-hotkeys
plan: "01"
subsystem: drawing
tags: [cpp, obs-plugin, freehand, mark, interpolation, pick-to-delete, hotkeys]

requires:
  - phase: 01-plugin-scaffold-and-core-rendering
    provides: Initial Mark, MarkList, ChalkSource, FreehandMark scaffolding with Phase 1 interfaces

provides:
  - Mark base class with distance_to (pure virtual) and update_end (virtual no-op) for all future mark types
  - ToolState with 5-tool enum and CHALK_PALETTE array with index-based color access
  - MarkList undo_mark() and delete_closest() methods under mutex
  - ChalkSource with all 9 hotkey ID fields, laser state, and pick_delete_mode
  - FreehandMark with linear interpolation (eliminates fast-motion gaps) and segment distance_to

affects:
  - 02-02 (new mark types Arrow/Circle/Cone must implement distance_to and may use update_end)
  - 02-03 (hotkey wiring uses all hotkey_* IDs in ChalkSource; tool routing uses ToolType enum and active_color())
  - all future mark types (must implement distance_to from base class)

tech-stack:
  added: []
  patterns:
    - "Pure virtual distance_to on Mark base — enables uniform pick-to-delete without type dispatch"
    - "Index-based palette (color_index + CHALK_PALETTE array) rather than raw ABGR value in ToolState"
    - "Linear interpolation in add_point with 2px step threshold to fill fast-motion gaps"
    - "Clamped projection formula for polyline segment distance — handles degenerate zero-length segments"

key-files:
  created: []
  modified:
    - src/marks/mark.hpp
    - src/tool-state.hpp
    - src/mark-list.hpp
    - src/mark-list.cpp
    - src/chalk-source.hpp
    - src/chalk-source.cpp
    - src/marks/freehand-mark.hpp
    - src/marks/freehand-mark.cpp

key-decisions:
  - "distance_to replaces hit_test on Mark base — returning float distance rather than bool enables pick-to-delete with threshold"
  - "CHALK_PALETTE lives at file scope (not inside ToolState struct) — shared constant, not per-instance data"
  - "update_end is a virtual no-op on Mark base — arrow/circle/cone override it; freehand and laser do not need it"
  - "color_index is int not uint8_t — avoids potential implicit conversion issues with CHALK_PALETTE_SIZE bounds checks"

patterns-established:
  - "Mark virtual interface: draw + distance_to (pure virtual) + add_point + update_end (no-op defaults)"
  - "All MarkList mutation methods lock mutex via std::lock_guard before touching marks vector"

requirements-completed: [TOOL-01]

duration: 2min
completed: 2026-03-30
---

# Phase 02 Plan 01: Expand Interfaces for Phase 2 Summary

**Phase 2 contract layer: Mark distance_to/update_end virtuals, 5-tool ToolState with CHALK_PALETTE, MarkList undo/delete_closest, ChalkSource hotkey fields, and freehand gap-filling via 2px linear interpolation**

## Performance

- **Duration:** ~2 min
- **Started:** 2026-03-30T17:53:20Z
- **Completed:** 2026-03-30T17:55:28Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments
- Established the Mark base class contract all Phase 2 mark types (arrow, circle, cone, laser) will implement — replacing the unused `hit_test` stub with the actually needed `distance_to` pure virtual and `update_end` no-op
- Expanded ToolState to the full 5-tool enum and file-scope CHALK_PALETTE array; `active_color()` method call replaces raw field access in chalk-source.cpp
- Added `undo_mark()` and `delete_closest()` to MarkList — both properly mutex-locked — providing the lifecycle operations Plan 03 hotkeys will wire up
- Added all 9 `obs_hotkey_id` fields, laser state, and `pick_delete_mode` to ChalkSource — Plan 03 can register hotkeys without structural changes
- Fixed freehand gap issue from Phase 1: `add_point()` now interpolates intermediate points at 2px intervals when mouse moves faster than one event per 2px

## Task Commits

1. **Task 1: Expand all interfaces for Phase 2** - `96f4e01` (feat)
2. **Task 2: Freehand interpolation and distance_to** - `3ed1db1` (feat)

## Files Created/Modified
- `src/marks/mark.hpp` - Added `distance_to` pure virtual and `update_end` virtual no-op; removed `hit_test`
- `src/tool-state.hpp` - 5-tool ToolType enum, CHALK_PALETTE array, `color_index` field, `active_color()` method
- `src/mark-list.hpp` - Added `undo_mark()` and `delete_closest()` declarations
- `src/mark-list.cpp` - Implemented `undo_mark()` (pop_back under lock) and `delete_closest()` (linear scan + erase under lock)
- `src/chalk-source.hpp` - Added 9 hotkey ID fields, `laser_active/laser_x/laser_y`, `pick_delete_mode`
- `src/chalk-source.cpp` - Updated `active_color` field access to `active_color()` method call (line 134)
- `src/marks/freehand-mark.hpp` - Replaced `hit_test override` with `distance_to override`
- `src/marks/freehand-mark.cpp` - Added interpolation to `add_point()`, implemented `distance_to()` with clamped projection

## Decisions Made
- `distance_to` returns `float` (minimum distance) rather than `bool` — this is what `delete_closest()` needs for threshold comparison. The old `hit_test` bool had no path forward for pick-to-delete.
- `CHALK_PALETTE` at file scope, not inside `ToolState` — it's a program-wide constant, not per-instance data.
- `update_end` added as virtual no-op (not pure virtual) — freehand and laser don't need it; making it pure virtual would force unnecessary overrides on every mark type.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] FreehandMark::hit_test override broken immediately by Task 1 interface change**
- **Found during:** Task 1 build verification
- **Issue:** Changing `hit_test` to `distance_to` on the Mark base class made FreehandMark's `hit_test override` a compiler error (no base function to override) and left `distance_to` unimplemented (pure virtual)
- **Fix:** Executed Task 2 immediately after Task 1 (before the Task 1 commit) — updated freehand-mark.hpp to declare `distance_to override` and implemented full Task 2 freehand logic in freehand-mark.cpp. Both tasks committed separately after the build succeeded.
- **Files modified:** src/marks/freehand-mark.hpp, src/marks/freehand-mark.cpp
- **Verification:** Build succeeded with `** BUILD SUCCEEDED **`
- **Committed in:** 3ed1db1 (Task 2 commit, separate from Task 1)

---

**Total deviations:** 1 auto-fixed (Rule 1 - sequential dependency between tasks)
**Impact on plan:** Task ordering was interleaved but both tasks completed correctly and committed separately. No scope creep.

## Issues Encountered
- Task 1 and Task 2 had a compile-time dependency: removing `hit_test` from the base class immediately broke `FreehandMark::hit_test override`. The plan anticipated this would be resolved by doing Task 2 after Task 1, but the build step in Task 1 couldn't pass until Task 2 was also done. Resolved by completing Task 2 before staging Task 1's commit, then committing both in order.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All contracts established for Plan 02 (new mark types): Arrow, Circle, and Cone must implement `distance_to` and may override `update_end`
- Plan 03 (hotkey wiring) has all the fields it needs in ChalkSource; ToolState enum is ready; MarkList undo/delete are implemented
- Freehand drawing is smoother — interpolation active immediately in the existing OBS plugin build

---
*Phase: 02-drawing-tools-mark-lifecycle-and-hotkeys*
*Completed: 2026-03-30*
