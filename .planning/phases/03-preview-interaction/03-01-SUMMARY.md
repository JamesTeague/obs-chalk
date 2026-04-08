---
phase: 03-preview-interaction
plan: 01
subsystem: ui
tags: [qt6, event-filter, coordinate-mapping, obs-frontend-api, cmake, hotkey]

requires:
  - phase: 02-drawing-tools-mark-lifecycle-and-hotkeys
    provides: "All 5 drawing tools with mark lifecycle and source-scoped hotkeys"

provides:
  - "ChalkEventFilter class: Qt event filter on OBS preview widget intercepts mouse and tablet events"
  - "preview_widget_to_scene coordinate mapping: widget pixels to OBS scene space"
  - "chalk_mode_install/shutdown lifecycle API wired into OBS frontend events"
  - "chalk_input_begin/move/end shared dispatch: called by both event filter and source callbacks"
  - "chalk_find_source: locates active ChalkSource from current OBS scene"
  - "Global hotkey 'Chalk: Toggle Chalk Mode' toggles chalk mode with crosshair cursor feedback"

affects:
  - 03-preview-interaction-02
  - phase-4-distribution

tech-stack:
  added:
    - "Qt6::Core and Qt6::Widgets (ENABLE_QT=true in CMakePresets.json)"
    - "obs-frontend-api (ENABLE_FRONTEND_API=true in CMakePresets.json)"
  patterns:
    - "Q_OBJECT class defined in .cpp file — requires #include 'chalk-mode.moc' at end of .cpp"
    - "Defer preview widget acquisition to OBS_FRONTEND_EVENT_FINISHED_LOADING (not obs_module_load)"
    - "Coordinate mapping: reimplemented GetScaleAndCenterPos + PREVIEW_EDGE_SIZE=10 inline (cannot include display-helpers.hpp from plugin)"
    - "Return true from tablet event handler to prevent Qt mouse synthesis (double dispatch pitfall)"
    - "obs_hotkey_register_frontend for global (session-level) hotkeys vs obs_hotkey_register_source for source-scoped"

key-files:
  created:
    - src/chalk-mode.hpp
    - src/chalk-mode.cpp
  modified:
    - CMakeLists.txt
    - CMakePresets.json
    - src/plugin.cpp
    - src/chalk-source.hpp
    - src/chalk-source.cpp

key-decisions:
  - "CMakePresets.json template preset had ENABLE_FRONTEND_API/QT hardcoded to false — overrode CMakeLists.txt options; fix is to set them true in the preset"
  - "Deps Qt6 (.deps/obs-deps-qt6-*) prl files reference AGL framework (removed in macOS 14+ SDK); fix by removing AGL from WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES after find_package(Qt6)"
  - "chalk_mode hotkey registered in chalk_mode_install (FINISHED_LOADING) per plan; documented that this means saved bindings won't persist across OBS restarts (move to obs_module_load to fix)"
  - "chalk_find_source uses obs_frontend_get_current_scene + obs_scene_enum_items — called per mouse event on Qt main thread (acceptable cost for v1)"
  - "Q_OBJECT in .cpp file requires chalk-mode.moc include at bottom for AUTOMOC to process it"

patterns-established:
  - "Pattern: Shared input dispatch (chalk_input_begin/move/end) enables both event filter and source callbacks to feed the same mark creation logic"
  - "Pattern: chalk_find_source is called per event from the filter — fine for mouse rates, but a cached version would be needed if called on every tablet move at 200Hz+"

requirements-completed: [PREV-01, PREV-02, PREV-03, PREV-04]

duration: 16min
completed: 2026-04-08
---

# Phase 3 Plan 01: Preview Interaction Summary

**Qt event filter on OBS preview widget with widget-to-scene coordinate mapping, global hotkey toggle, and crosshair cursor — drawing input now intercepted directly from preview rather than requiring source selection**

## Performance

- **Duration:** 16 min
- **Started:** 2026-04-08T19:00:19Z
- **Completed:** 2026-04-08T19:16:19Z
- **Tasks:** 2
- **Files modified:** 6 (plus 2 created)

## Accomplishments
- Extracted `chalk_input_begin/move/end` shared dispatch functions from source callbacks, enabling both the event filter and source interaction callbacks to feed the same mark creation logic
- Created `ChalkEventFilter` (Q_OBJECT in .cpp) that intercepts mouse and tablet events on the OBS preview widget, mapping coordinates from widget pixels to scene pixels via inline reimplementation of `GetScaleAndCenterPos`
- Wired `chalk_mode_install/shutdown` into OBS frontend event lifecycle via `plugin.cpp` callback on `OBS_FRONTEND_EVENT_FINISHED_LOADING`
- Registered global hotkey "Chalk: Toggle Chalk Mode" via `obs_hotkey_register_frontend` with crosshair cursor toggle
- Fixed two macOS build blockers: CMakePresets.json hardcoded ENABLE options to false (overriding CMakeLists.txt), and deps Qt6 .prl files reference removed AGL framework

## Task Commits

Each task was committed atomically:

1. **Task 1: Extract shared input dispatch and enable build system** - `65d9efb` (feat)
2. **Task 2: Create chalk-mode module with event filter, coordinate mapping, hotkey, cursor** - `87fde8d` (feat)

## Files Created/Modified
- `src/chalk-mode.hpp` - Public lifecycle API: `chalk_mode_install`, `chalk_mode_shutdown`
- `src/chalk-mode.cpp` - Full implementation: ChalkEventFilter (Q_OBJECT), coordinate mapping, hotkey, cursor toggle
- `src/plugin.cpp` - Added frontend event callback; obs_module_unload removes it
- `src/chalk-source.hpp` - Added `chalk_input_begin/move/end` and `chalk_find_source` declarations
- `src/chalk-source.cpp` - Implemented shared dispatch functions; refactored `chalk_mouse_click/move` to delegate to them
- `CMakeLists.txt` - Added frontend API include dir fix, AGL removal from WrapOpenGL deps
- `CMakePresets.json` - Flipped ENABLE_FRONTEND_API and ENABLE_QT from false to true in template preset

## Decisions Made

**CMakePresets.json preset override:** The `template` preset had `cacheVariables` setting `ENABLE_FRONTEND_API: false` and `ENABLE_QT: false`. CMake cache variables override `option()` defaults in CMakeLists.txt, so our changes to CMakeLists.txt had no effect. Fix: change those to `true` in the preset.

**AGL framework removal:** The deps Qt6 (built before macOS 14) includes AGL in `WrapOpenGL::WrapOpenGL`'s link dependencies via `.prl` files. macOS 14+ SDK doesn't include AGL. OBS.app's own Qt6 doesn't reference AGL. Fix: after `find_package(Qt6)`, remove `/System/Library/Frameworks/AGL.framework` from `WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES`. This needs to happen after clean cmake configure (stale Xcode project caches the old link flags).

**Hotkey registration timing:** Plan calls for registering in `chalk_mode_install` (FINISHED_LOADING). Research Pitfall 6 notes this means saved bindings won't be restored on OBS restart. Documented in code with TODO comment. Functionally correct for v1 — binding just needs to be re-set after first install.

**chalk_find_source per-event:** Called on each mouse event from the event filter. At mouse rates (~60Hz) this is fine. Tablet rates can be 200Hz+; if that proves expensive, add a cached version with scene-change invalidation.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] CMakePresets.json hardcoded ENABLE flags to false**
- **Found during:** Task 1 (cmake configure)
- **Issue:** `template` preset's `cacheVariables` set `ENABLE_FRONTEND_API: false` and `ENABLE_QT: false`, overriding the `ON` values in CMakeLists.txt `option()` — none of the Qt/frontend-api code compiled
- **Fix:** Changed both to `true` in CMakePresets.json template preset
- **Files modified:** CMakePresets.json
- **Verification:** cmake configure picks up Qt6 and obs-frontend-api; xcodeproj includes correct headers
- **Committed in:** 87fde8d (Task 2 commit)

**2. [Rule 3 - Blocking] Deps Qt6 AGL framework link failure on macOS 14+ SDK**
- **Found during:** Task 2 (linker phase)
- **Issue:** Qt6 was built with AGL framework dependency (in WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES); AGL was removed from macOS 14+ SDK; link fails with "framework 'AGL' not found"
- **Fix:** After find_package(Qt6), get and modify INTERFACE_LINK_LIBRARIES of WrapOpenGL::WrapOpenGL to remove AGL; requires clean cmake configure (not just rebuild)
- **Files modified:** CMakeLists.txt
- **Verification:** xcodeproj no longer references AGL; build succeeds with zero errors and zero warnings
- **Committed in:** 87fde8d (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (both Rule 3 — blocking build issues)
**Impact on plan:** Both fixes required for the build to work at all on macOS with Xcode 26 beta. No scope creep — no new functionality added, only build correctness.

## Issues Encountered

The `#include "chalk-mode.moc"` at the end of `chalk-mode.cpp` is required by AUTOMOC when Q_OBJECT is defined in a .cpp file (not a header). If omitted, MOC doesn't find the class definition and signal/slot connectivity silently fails. This was anticipated in the plan and handled correctly.

## Next Phase Readiness
- Plugin builds with Qt6 and obs-frontend-api; event filter code is in place
- Functional verification (OBS launch, hotkey binding, drawing on preview) deferred to plan 03 checkpoint per phase plan structure
- Plan 02 (tablet pressure) can now read tablet events from `QTabletEvent::pressure()` in the existing `handle_tablet()` method — just needs the pressure to be plumbed through to `chalk_input_begin`
- Known limitation: hotkey binding not persisted across OBS restarts (see decisions section)

## Self-Check: PASSED

All key files verified present. Both task commits (65d9efb, 87fde8d) confirmed in git log. Build verified: `** BUILD SUCCEEDED **` with zero errors and zero warnings.

---
*Phase: 03-preview-interaction*
*Completed: 2026-04-08*
