# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-23)

**Core value:** Coaches can reliably draw on film during a livestream without the tool crashing or disrupting the broadcast.
**Current focus:** Phase 1 — Plugin Scaffold and Core Rendering

## Current Position

Phase: 1 of 4 (Plugin Scaffold and Core Rendering)
Plan: 1 of TBD in current phase (awaiting Task 3 human-verify checkpoint)
Status: In progress — checkpoint:human-verify at Task 3
Last activity: 2026-03-30 — Plan 01-01 tasks 1-2 complete, awaiting OBS load verification

Progress: [█░░░░░░░░░] 5%

## Performance Metrics

**Velocity:**
- Total plans completed: 0
- Average duration: —
- Total execution time: —

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**
- Last 5 plans: —
- Trend: —

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Object model over pixel painting — enables selective deletion and avoids the GPU race that crashes obs-draw
- Source type is OBS_SOURCE_TYPE_INPUT — required for preview mouse interaction and independent overlay positioning; must be correct from first commit
- Qt version must match OBS binary exactly (Qt 6.8.3 in OBS 32.x); link against /Applications/OBS.app/Contents/Frameworks
- cmake --preset macos auto-downloads obs-studio 31.1.1 + prebuilt deps into .deps/ at configure time — this is the obs-plugintemplate flow
- xcodebuild -runFirstLaunch required on Xcode 26 beta to fix IDESimulatorFoundation symbol crash before cmake configure succeeds
- All gs_* calls confined to chalk_video_render — established in Plan 01, must be maintained in all future plans

### Pending Todos

None yet.

### Blockers/Concerns

- Qt version pinning: verify exact Qt subversion by inspecting /Applications/OBS.app/Contents/Frameworks before writing CMakeLists.txt
- Phase 4: macOS codesigning requires Developer ID cert configuration in CI before first public release — plan a spike

## Session Continuity

Last session: 2026-03-30
Stopped at: Plan 01-01 Tasks 1-2 complete (build system + plugin source). Awaiting Task 3 human verification (OBS load test) at checkpoint:human-verify.
Resume file: .planning/phases/01-plugin-scaffold-and-core-rendering/01-01-PLAN.md (Task 3)
