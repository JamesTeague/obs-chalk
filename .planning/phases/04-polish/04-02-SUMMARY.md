---
phase: 04-polish
plan: 02
subsystem: drawing-ux
tags: [obs-plugin, cpp, hotkeys, drawing-tools, laser, cone, scene-transition]

requires:
  - phase: 04-polish-01
    provides: thick-stroke rendering for ArrowMark/CircleMark/ConeMark

provides:
  - Cone fill renders at 0.35 alpha (transparent, canvas visible through interior)
  - Pick-to-delete mode stays active across multiple clicks until hotkey toggled off
  - Laser pointer is a selectable tool (hotkey_tool_laser); mouse-down shows dot, mouse-up hides
  - clear_on_scene_change setting: when enabled, scene switch clears all marks

affects:
  - 04-polish-03 (verification checkpoint)
  - Any future UI work touching hotkey registration pattern

tech-stack:
  added: []
  patterns:
    - obs_source_info fields must be in struct declaration order (C++ designated initializer constraint)
    - chalk_update reads obs_data_t settings; called from chalk_create for initial values
    - Tool selection hotkeys use chalk_hotkey_tool_X pattern (on-press only, no release handling)

key-files:
  created: []
  modified:
    - src/chalk-source.cpp
    - src/chalk-source.hpp
    - src/plugin.cpp

key-decisions:
  - "chalk_hotkey_laser (hold-key) removed; replaced with chalk_hotkey_tool_laser (tool selection, press-only)"
  - "laser_active guarded by active_tool==Laser in video_render to prevent dot persisting after mid-stroke tool switch"
  - "obs_source_info fields get_defaults/get_properties/update inserted between get_height and video_render per struct declaration order"

patterns-established:
  - "Settings pattern: get_defaults sets defaults, get_properties adds UI checkbox, update reads into ChalkSource field"

requirements-completed: [UX-02, UX-03, UX-04, INTG-01]

duration: 5min
completed: 2026-04-09
---

# Phase 4 Plan 2: UX Polish and Scene-Transition Clear Summary

**Cone 0.35f alpha, persistent pick-delete, laser refactored to selectable tool with mouse-down/up activation, and optional clear-on-scene-transition via OBS source settings**

## Performance

- **Duration:** 5 min
- **Started:** 2026-04-09T03:00:11Z
- **Completed:** 2026-04-09T03:05:21Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- Cone fill is visibly transparent at 0.35 alpha — canvas content visible through the cone interior (UX-02)
- Pick-to-delete stays active across multiple marks without re-toggling the hotkey (UX-03)
- Laser pointer refactored from hold-key to selectable tool: mouse-down shows dot, mouse-up hides; `chalk_hotkey_tool_laser` registered as `chalk.tool.laser` (UX-04)
- `clear_on_scene_change` OBS source setting: checkbox in source properties; when enabled, `OBS_FRONTEND_EVENT_SCENE_CHANGED` triggers `clear_all()` (INTG-01)

## Task Commits

Each task was committed atomically:

1. **Task 1: Cone alpha, persistent delete, laser-as-tool** - `5b87b24` (feat)
2. **Task 2: Clear-on-scene-transition setting** - `e5c2440` (feat)

**Plan metadata:** (final commit follows this summary)

## Files Created/Modified

- `/Users/jteague/dev/obs-chalk/src/chalk-source.cpp` - Cone 0.35f alpha; remove pick_delete auto-reset; laser input begin/end refactored; chalk_hotkey_laser removed, chalk_hotkey_tool_laser added; chalk_update/get_defaults/get_properties callbacks added and wired into chalk_source_info
- `/Users/jteague/dev/obs-chalk/src/chalk-source.hpp` - hotkey_laser replaced with hotkey_tool_laser; clear_on_scene_change field added
- `/Users/jteague/dev/obs-chalk/src/plugin.cpp` - Added #include "chalk-source.hpp"; added SCENE_CHANGED branch calling clear_all when setting enabled

## Decisions Made

- `chalk_hotkey_laser` (hold-key, pressed/released) removed entirely; replaced with `chalk_hotkey_tool_laser` (press-only, sets active_tool = Laser). The hold behavior is now implemented via input begin/end: mouse-down shows dot, mouse-up hides.
- Laser dot in `chalk_video_render` now guarded by `active_tool == ToolType::Laser` to prevent dot from persisting if tool changes mid-stroke.
- `obs_source_info` fields `get_defaults`/`get_properties`/`update` placed between `get_height` and `video_render` to satisfy C++ designated initializer ordering requirement (Werror enforced).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed obs_source_info designated initializer order**
- **Found during:** Task 2 (wiring chalk_get_defaults/get_properties/update into chalk_source_info)
- **Issue:** Plan placed the three new fields after `mouse_move` in the struct initializer, but C++ requires designated initializers in struct declaration order; `get_defaults` (283) comes before `video_render` (351) in obs_source_info
- **Fix:** Reordered to: get_width, get_height, get_defaults, get_properties, update, video_render, mouse_click, mouse_move
- **Files modified:** src/chalk-source.cpp
- **Verification:** Build succeeded with -Werror after reorder
- **Committed in:** e5c2440 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - bug)
**Impact on plan:** Required for build correctness. No scope creep.

## Issues Encountered

- Some Task 1 changes (cone alpha, persistent delete, laser input begin/end) were already committed to HEAD from a prior session. The Task 1 commit (`5b87b24`) contains the remaining pieces: `chalk_hotkey_tool_laser` function, hotkey key string change (`chalk.tool.laser`), and `active_tool == Laser` render guard.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- All four behavioral changes are in place and build clean
- Ready for Phase 4 Plan 3 (verification checkpoint) — all five verifiable behaviors are testable in OBS:
  1. Cone fill appears transparent
  2. Pick-delete allows multiple marks without re-toggling
  3. Laser tool: mouse-down shows dot, mouse-up hides
  4. Source properties show ClearOnSceneChange checkbox
  5. Scene switch clears marks when setting enabled

---
*Phase: 04-polish*
*Completed: 2026-04-09*

## Self-Check: PASSED

- FOUND: .planning/phases/04-polish/04-02-SUMMARY.md
- FOUND: src/chalk-source.cpp
- FOUND: src/chalk-source.hpp
- FOUND: src/plugin.cpp
- FOUND commit: 5b87b24 (feat(04-02): laser-as-tool)
- FOUND commit: e5c2440 (feat(04-02): clear-on-scene-transition)
