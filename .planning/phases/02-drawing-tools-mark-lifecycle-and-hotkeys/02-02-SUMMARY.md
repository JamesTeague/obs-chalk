---
phase: 02-drawing-tools-mark-lifecycle-and-hotkeys
plan: "02"
subsystem: mark-types
tags: [rendering, marks, arrow, circle, cone, gs-rendering]
dependency_graph:
  requires: [02-01]
  provides: [ArrowMark, CircleMark, ConeMark]
  affects: [02-03]
tech_stack:
  added: []
  patterns: [GS_LINESTRIP for outlines, GS_TRIS for filled shapes, GS_NEITHER cull mode for double-sided fill]
key_files:
  created:
    - src/marks/arrow-mark.hpp
    - src/marks/arrow-mark.cpp
    - src/marks/circle-mark.hpp
    - src/marks/circle-mark.cpp
    - src/marks/cone-mark.hpp
    - src/marks/cone-mark.cpp
  modified:
    - CMakeLists.txt
decisions:
  - "ArrowMark arrowhead uses angle +/- PI from shaft direction rather than +/-30deg from shaft — cleaner trig: arms extend backward from head at +/- 30deg"
  - "ConeMark stores dragged corner (cx_,cy_) directly, computes corner2 via reflection helper at draw/distance_to time — avoids stale data"
  - "ConeMark compute_corner2 extracted as private method shared between draw() and distance_to() — single source of truth for reflection math"
metrics:
  duration_seconds: 110
  completed_date: "2026-03-30"
  tasks_completed: 2
  files_created: 6
  files_modified: 1
---

# Phase 02 Plan 02: Arrow, Circle, and Cone Mark Types Summary

Three new mark types implementing the Mark base interface for draw, distance_to, and update_end — completing the mark type library before Plan 03 wires tools to mouse interaction.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | ArrowMark and CircleMark | c3f0b23 | arrow-mark.hpp/cpp, circle-mark.hpp/cpp, CMakeLists.txt |
| 2 | ConeMark | 2062dbd | cone-mark.hpp/cpp, CMakeLists.txt |

## What Was Built

**ArrowMark** renders a directed arrow via three GS_LINESTRIP calls: one for the shaft (tail to head) and two for the V-arrowhead arms (each 15px, at +/- 30 degrees from the reversed shaft direction). `distance_to` applies the standard point-to-segment formula on the shaft. `update_end` moves the head, enabling drag preview.

**CircleMark** renders a 64-segment closed circle outline as GS_LINESTRIP (65 vertices including the closing point). `distance_to` returns `abs(hypot(x-cx, y-cy) - r)` — the natural ring distance. `update_end` sets radius via `hypot(x - cx_, y - cy_)`.

**ConeMark** renders a filled isosceles triangle via GS_TRIS with `GS_NEITHER` cull mode for double-sided visibility. The apex is the click point; the dragged corner is corner1; corner2 is the reflection of corner1 across the axis (locked on the first `update_end` call exceeding 5px from apex). No hardcoded angle — cone shape is fully determined by drag position relative to the axis. `distance_to` handles both edge proximity (point-to-segment on all three edges) and interior containment (cross-product sign test returns 0).

## Deviations from Plan

None — plan executed exactly as written.

## Self-Check

Files created:
- src/marks/arrow-mark.hpp: FOUND
- src/marks/arrow-mark.cpp: FOUND
- src/marks/circle-mark.hpp: FOUND
- src/marks/circle-mark.cpp: FOUND
- src/marks/cone-mark.hpp: FOUND
- src/marks/cone-mark.cpp: FOUND

Commits:
- c3f0b23: FOUND
- 2062dbd: FOUND

Build: PASSED (cmake --build build_macos --config RelWithDebInfo succeeded)
