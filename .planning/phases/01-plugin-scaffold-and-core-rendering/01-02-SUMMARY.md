---
phase: 01-plugin-scaffold-and-core-rendering
plan: "02"
subsystem: rendering
tags: [obs, cpp, opengl, graphics, freehand, mutex, threading]

requires:
  - phase: 01-plugin-scaffold-and-core-rendering/01-01
    provides: Plugin scaffold with OBS callbacks registered, ChalkSource struct, transparent overlay rendering loop

provides:
  - Mark abstract base class with virtual draw/hit_test/add_point
  - FreehandMark concrete type storing points and drawing via gs_render_start/vertex2f/render_stop
  - MarkList thread-safe container with mutex protecting all access paths
  - Wired mouse callbacks: left-click begins mark, drag adds points, release commits, right-click clears all
  - video_render draws all committed marks plus in_progress stroke under mutex lock
  - Verified end-to-end: freehand strokes visible in OBS preview, clear-all stable under rapid draw-clear cycles

affects:
  - 01-03 (tool switching, hotkeys, color picker — builds on MarkList and FreehandMark)
  - 02-x (line interpolation — fills gaps produced by fast mouse movement, deferred from this plan)
  - All future mark types (inherit from Mark base class)

tech-stack:
  added: []
  patterns:
    - "Object model rendering: marks stored as std::unique_ptr<Mark> in MarkList, drawn per-frame in video_render — enables selective deletion and avoids GPU race class"
    - "Thread safety contract: MarkList mutex locked in all write paths (mouse callbacks) and all read paths (video_render); no gs_* calls outside render path"
    - "Polymorphic point addition: add_point() virtual with default no-op allows mouse_move to call it on in_progress without downcasting"
    - "ABGR color unpacking: color_uint32_to_rgba helper converts OBS uint32 ABGR to float r,g,b,a for FreehandMark constructor"

key-files:
  created:
    - src/marks/mark.hpp
    - src/marks/freehand-mark.hpp
    - src/marks/freehand-mark.cpp
    - src/mark-list.hpp
    - src/mark-list.cpp
  modified:
    - src/chalk-source.hpp
    - src/chalk-source.cpp
    - CMakeLists.txt

key-decisions:
  - "Right-click button type is 2 (MOUSE_RIGHT), not 1 (MOUSE_MIDDLE) — OBS SDK type enum uses 0=left, 1=middle, 2=right"
  - "drawing flag on ChalkSource is not mutex-protected — mouse callbacks are serialized by OBS; only mark_list mutations need the lock"
  - "in_progress mark drawn live in video_render so user sees stroke while dragging, not just on commit"
  - "Line interpolation (fill gaps on fast mouse movement) deferred to Phase 2 — raw mouse events are sufficient for Phase 1 verification"

patterns-established:
  - "Pattern: All gs_* calls confined to chalk_video_render and mark draw() methods — never in mouse or hotkey callbacks"
  - "Pattern: MarkList is the sole mutation interface for the mark collection — all operations lock mutex before modifying"

requirements-completed: [CORE-02, CORE-03, CORE-04]

duration: ~20min
completed: "2026-03-30"
---

# Phase 1 Plan 02: Mark Object Model and Freehand Drawing Summary

**Freehand drawing over OBS preview using mutex-protected Mark/MarkList object model with gs_render_start per-frame rendering**

## Performance

- **Duration:** ~20 min
- **Started:** 2026-03-30T09:54:19-05:00
- **Completed:** 2026-03-30T10:07:57-05:00
- **Tasks:** 3 (2 auto + 1 human-verify)
- **Files modified:** 8

## Accomplishments

- Mark abstract base class and FreehandMark concrete type implementing the vector-object rendering model that eliminates the GPU race class from obs-draw
- MarkList with std::mutex protecting all read and write paths — mouse callbacks write, video_render reads, no gs_* calls in callbacks
- Verified in OBS: freehand strokes visible on next frame, clear-all stable under rapid draw-clear cycles, no crash or GPU artifacts

## Task Commits

Each task was committed atomically:

1. **Task 1: Create Mark base class, FreehandMark, and MarkList** — `e6054a7` (feat)
2. **Task 2: Wire mouse callbacks and video_render** — `a6a949a` (feat)
3. **Deviation fix: Correct right-click button type** — `2b3d90e` (fix)
4. **Task 3: Human-verify checkpoint (approved)** — no code commit; verification confirmed in OBS

## Files Created/Modified

- `src/marks/mark.hpp` — Abstract Mark base class: virtual draw(gs_eparam_t*), hit_test(float,float), add_point(float,float) no-op default
- `src/marks/freehand-mark.hpp` — FreehandMark: vector of FreehandPoint, vec4 color, overrides draw/add_point/hit_test
- `src/marks/freehand-mark.cpp` — draw() via gs_render_start/gs_vertex2f/gs_render_stop(GS_LINESTRIP), hit_test returns false (Phase 2)
- `src/mark-list.hpp` — MarkList: marks vector, in_progress unique_ptr, mutable mutex, begin_mark/commit_mark/clear_all
- `src/mark-list.cpp` — All MarkList operations under std::lock_guard<std::mutex>
- `src/chalk-source.hpp` — Added MarkList mark_list member, bool drawing flag
- `src/chalk-source.cpp` — Mouse callbacks wired: begin/commit marks, add points, clear_all on right-click; video_render draws under lock
- `CMakeLists.txt` — Added freehand-mark.cpp and mark-list.cpp to build sources

## Decisions Made

- **Right-click button type is 2, not 1.** OBS SDK MOUSE_RIGHT=2, MOUSE_MIDDLE=1. Plan spec said "type==2" but comment said "middle-click" — actual right-click needed for clear-all trigger. Fixed in deviation commit.
- **drawing flag not mutex-protected.** Mouse callbacks are serialized by OBS's interaction dispatch, so the flag does not need locking. Only mark_list mutations cross thread boundaries.
- **Line interpolation deferred to Phase 2.** Fast mouse movement produces visible gaps in strokes. This is expected behavior for raw mouse events and does not block Phase 1 verification.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Right-click button type was wrong (MOUSE_MIDDLE vs MOUSE_RIGHT)**
- **Found during:** Task 3 (human-verify checkpoint) — user reported clear-all not triggering on right-click
- **Issue:** Plan comment said "middle-click (type==2)" but OBS SDK defines MOUSE_MIDDLE=1 and MOUSE_RIGHT=2. The code used type==1 which only triggered on middle-click, not right-click.
- **Fix:** Changed clear-all trigger from `type == 1` to `type == 2` in chalk_mouse_click()
- **Files modified:** src/chalk-source.cpp
- **Verification:** User confirmed right-click now clears all marks in OBS preview
- **Committed in:** 2b3d90e

---

**Total deviations:** 1 auto-fixed (Rule 1 - bug)
**Impact on plan:** Necessary correctness fix. No scope creep. Clear-all now correctly responds to right-click as intended.

### Deferred Items

- **Line interpolation (gaps on fast mouse movement):** Strokes miss points when moving quickly — expected limitation of raw mouse events. Deferred to Phase 2. Logged as known behavior, not a defect.

## Issues Encountered

- OBS SDK button type enum was not clearly documented in the plan's interface spec — `type==2` in a comment labeled "middle-click" caused the wrong button type to be used. Fixed before plan completion.

## User Setup Required

None — plugin installation path established in Plan 01-01 (`/Applications/OBS.app/Contents/PlugIns/obs-chalk.plugin`). No new external configuration required.

## Next Phase Readiness

- Core rendering loop is proven end-to-end: mouse input -> MarkList -> video_render -> visible stroke
- Mark/MarkList architecture is stable and ready for tool switching, color picker, and hotkey wiring in Plan 01-03
- hit_test() stubbed (returns false) — Phase 2 implements pick-to-delete geometry
- Line interpolation deferred — Phase 2 fills stroke gaps on fast mouse movement

---
*Phase: 01-plugin-scaffold-and-core-rendering*
*Completed: 2026-03-30*

## Self-Check: PASSED

All claimed files found on disk. All claimed commits found in git history.
