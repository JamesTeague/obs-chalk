---
phase: 01-plugin-scaffold-and-core-rendering
plan: 01
subsystem: infra
tags: [obs-plugin, cmake, xcode, c++, obs-plugintemplate, libobs]

# Dependency graph
requires: []
provides:
  - obs-chalk plugin binary (universal macOS arm64+x86_64) that registers as OBS_SOURCE_TYPE_INPUT
  - CMake build system with obs-plugintemplate scaffolding and OBS.app framework path
  - ChalkSource struct with transparent overlay render loop using SOLID effect
  - ToolState and ToolType stubs for Phase 2 drawing
  - chalk_source_info registered with OBS_SOURCE_VIDEO, OBS_SOURCE_INTERACTION, OBS_SOURCE_CUSTOM_DRAW
affects: [02, 03, 04]

# Tech tracking
tech-stack:
  added: [cmake 4.3.1, obs-studio 31.1.1 (via .deps auto-download), clang/clang++ (Xcode 26), obs-plugintemplate scaffold]
  patterns: [all gs_* calls confined to video_render callback, get_width/get_height via obs_get_video_info, structured obs_source_info registration]

key-files:
  created:
    - src/plugin.cpp
    - src/chalk-source.cpp
    - src/chalk-source.hpp
    - src/tool-state.hpp
    - data/locale/en-US.ini
    - CMakeLists.txt
    - CMakePresets.json
    - buildspec.json
    - .gitignore
  modified: []

key-decisions:
  - "OBS_SOURCE_TYPE_INPUT confirmed over FILTER — required for independent overlay positioning and mouse interaction in all future phases"
  - "OBS_SOURCE_CUSTOM_DRAW required for correct alpha compositing — transparent overlay established in Phase 1 render loop"
  - "cmake --preset macos downloads obs-studio 31.1.1 + prebuilt deps into .deps/ at configure time — this is expected, not a bug"
  - "xcodebuild -runFirstLaunch required on Xcode 26 beta to fix IDESimulatorFoundation symbol crash before cmake configure succeeds"
  - "cmake 4.3.1 installed (above plan's 3.28-3.30 range) — no issues encountered, configure and build succeed"

patterns-established:
  - "gs_* calls: all graphics API calls confined to chalk_video_render; never call obs_enter_graphics/obs_leave_graphics (already in graphics context)"
  - "Source dimensions: always read from obs_get_video_info().base_width/base_height, not hardcoded"
  - "Blend state: always push/pop around draw calls, use GS_BLEND_SRCALPHA + GS_BLEND_INVSRCALPHA"

requirements-completed: [CORE-01, CORE-05]

# Metrics
duration: ~60min
completed: 2026-03-30
---

# Phase 01 Plan 01: Plugin Scaffold and Core Rendering Summary

**OBS plugin binary builds cleanly as universal macOS bundle (arm64+x86_64) with OBS_SOURCE_TYPE_INPUT transparent overlay source registered as "Chalk Drawing"**

## Performance

- **Duration:** ~60 min
- **Started:** 2026-03-30T13:26:24Z
- **Completed:** 2026-03-30T14:30:00Z
- **Tasks:** 2 of 3 complete (Task 3 is human-verify checkpoint)
- **Files modified:** 8 source/config files created

## Accomplishments

- Build system configured via obs-plugintemplate: CMake 4.3.1 + Xcode generator, universal binary (arm64+x86_64), ENABLE_QT=OFF, ENABLE_FRONTEND_API=OFF, OBS.app framework prefix path set for Phase 3 Qt work
- Plugin source scaffold: plugin.cpp with OBS_DECLARE_MODULE, chalk-source.cpp with obs_source_info (OBS_SOURCE_TYPE_INPUT, VIDEO+INTERACTION+CUSTOM_DRAW flags), transparent blend state render loop
- Plugin binary at `build_macos/RelWithDebInfo/obs-chalk.plugin` builds cleanly, codesigned for local run
- All gs_* calls confined to chalk_video_render — invariant established for all future phases

## Task Commits

1. **Task 1: Bootstrap build system from obs-plugintemplate** - `b2639ea` (chore)
2. **Task 2: Plugin entry point and ChalkSource transparent overlay** - `bb504e1` (feat)
3. **Task 3: Verify plugin loads in OBS** - awaiting human verification

## Files Created/Modified

- `src/plugin.cpp` - OBS_DECLARE_MODULE, obs_register_source, obs_module_load/unload
- `src/chalk-source.cpp` - chalk_source_info struct, all callbacks, transparent blend state render
- `src/chalk-source.hpp` - ChalkSource struct with obs_source_t* and ToolState
- `src/tool-state.hpp` - ToolType enum and ToolState (Phase 1 stub)
- `data/locale/en-US.ini` - obs-chalk="Chalk Drawing"
- `CMakeLists.txt` - project obs-chalk, C++17, ENABLE_QT=OFF, macOS prefix path
- `CMakePresets.json` - macos preset with Xcode generator, universal binary
- `buildspec.json` - obs-chalk 0.1.0, obs-studio 31.1.1 dependency
- `.gitignore` - excludes build_macos/, .deps/

## Decisions Made

- OBS_SOURCE_TYPE_INPUT confirmed as correct source type — filter sources cannot be independently positioned or receive mouse input; this invariant carries through all future plans
- cmake --preset macos auto-downloads obs-studio source and prebuilt deps into .deps/ at configure time; this is the obs-plugintemplate flow, not a workaround
- xcodebuild -runFirstLaunch was required on this machine (Xcode 26 beta) to fix a DVTDownloads/IDESimulatorFoundation symbol crash that prevented CMake's Xcode generator compiler test from passing

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added .gitignore to exclude build artifacts and .deps/**
- **Found during:** Task 1 (commit staging)
- **Issue:** build_macos/ and .deps/ are large generated artifacts (OBS source download + build) that should not be committed
- **Fix:** Created .gitignore excluding build_macos/, build_x64/, build_x86_64/, .deps/, .DS_Store
- **Files modified:** .gitignore (new)
- **Verification:** git status confirmed neither directory appears as untracked after .gitignore added
- **Committed in:** b2639ea (Task 1 commit)

**2. [Rule 3 - Blocking] Ran xcodebuild -runFirstLaunch to fix Xcode 26 simulator plugin crash**
- **Found during:** Task 1 (cmake --preset macos configure step)
- **Issue:** CMake Xcode generator compiler test failed because xcodebuild crashed loading IDESimulatorFoundation.framework (missing DVTDownloads symbol, Xcode 26 beta issue)
- **Fix:** Ran `xcodebuild -runFirstLaunch` which completed "Install Succeeded" and registered the correct simulator service version
- **Files modified:** System-level Xcode framework registration (no repo files)
- **Verification:** cmake --preset macos succeeded after fix, downloaded and built OBS deps, generated Xcode project
- **Committed in:** n/a (system fix)

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both fixes were necessary infrastructure. No scope creep.

## Issues Encountered

- cmake 4.3.1 is above the plan's documented 3.28-3.30 range. No compatibility issues encountered — configure and build succeed cleanly.
- OBS.app is not installed at `/Applications/OBS.app/` on this machine. The CMakeLists.txt sets it as a prefix path (correct for Phase 3 Qt linking), but configure and build work without it since cmake downloads its own copy of libobs via buildspec.json.

## User Setup Required

**To test the plugin in OBS (Task 3 checkpoint):**

1. Copy the built plugin bundle to the OBS plugins directory:
   ```
   cp -r /Users/jteague/dev/obs-chalk/build_macos/RelWithDebInfo/obs-chalk.plugin \
     ~/Library/Application\ Support/obs-studio/plugins/obs-chalk/bin/
   ```
   (Create the `bin/` subdirectory first if it doesn't exist)

2. Launch OBS Studio
3. Sources panel → "+" → verify "Chalk Drawing" appears
4. Add the source → verify full-canvas transparent overlay (sources behind it visible)
5. Right-click source → "Interact" → interaction window should open
6. Check OBS log (Help > Log Files) for any load errors

## Next Phase Readiness

- Build system and source registration invariants established — Plan 02 can add MarkList and freehand drawing
- OBS_SOURCE_TYPE_INPUT, OBS_SOURCE_CUSTOM_DRAW, OBS_SOURCE_INTERACTION flags confirmed in chalk_source_info
- gs_* confinement pattern established: all graphics calls stay inside chalk_video_render
- Awaiting Task 3 human verification of OBS plugin load before marking Plan 01 complete

---
*Phase: 01-plugin-scaffold-and-core-rendering*
*Completed: 2026-03-30*
