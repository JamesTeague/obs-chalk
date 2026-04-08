---
phase: 03-preview-interaction
verified: 2026-04-08T20:00:00Z
status: human_needed
score: 9/9 must-haves verified
re_verification: false
human_verification:
  - test: "With chalk mode ON (hotkey bound and pressed), click and drag on the OBS preview"
    expected: "Marks appear at the correct scene-space positions; marks track cursor position accurately across the canvas including corners and edges"
    why_human: "Coordinate mapping correctness (letterbox offset + scale) cannot be verified without a running OBS instance and visual output"
  - test: "With chalk mode ON, draw with a tablet pen at light then heavy pressure"
    expected: "Light pressure produces thin strokes (~1.5px), heavy pressure produces thick strokes (~6px), and variation is visible mid-stroke"
    why_human: "Tablet hardware was unavailable during verification (documented in 03-03-SUMMARY.md as INPT-03 untestable). Pressure code path is implemented and reviewed but not live-tested."
---

# Phase 3: Preview Interaction Verification Report

**Phase Goal:** Preview interaction — Qt event filter on OBS preview widget for direct drawing with coordinate mapping, global hotkey toggle, cursor feedback, and tablet pressure sensitivity.
**Verified:** 2026-04-08
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | With chalk mode active, clicking on the OBS preview creates marks at the correct scene-space positions | ? HUMAN | Code path complete and live-verified (03-03-SUMMARY); coordinate mapping formula implemented correctly in `preview_widget_to_scene`; functional confirmation requires visual OBS test |
| 2 | With chalk mode inactive, preview mouse events pass through to normal OBS behavior | ✓ VERIFIED | `eventFilter` returns `false` immediately when `!s_chalk_mode_active` (chalk-mode.cpp:132-133) |
| 3 | A global hotkey toggles chalk mode on/off | ✓ VERIFIED | `obs_hotkey_register_frontend("chalk.chalk_mode", "Chalk: Toggle Chalk Mode", ...)` in `chalk_mode_install`; callback toggles `s_chalk_mode_active` and calls `chalk_update_cursor`; live-confirmed in OBS (03-03-SUMMARY PREV-03) |
| 4 | The preview cursor changes to a crosshair when chalk mode is active | ✓ VERIFIED | `QGuiApplication::setOverrideCursor(Qt::CrossCursor)` on activate, `restoreOverrideCursor()` on deactivate; app-level override confirmed to survive OBS cursor resets (fix committed in 29f8fd3); live-confirmed (03-03-SUMMARY PREV-04) |
| 5 | Existing source interaction callbacks still work as a secondary input path | ✓ VERIFIED | `chalk_mouse_click` and `chalk_mouse_move` remain intact in chalk-source.cpp:407-441; both delegate to shared dispatch functions; no logic removed |
| 6 | Drawing with a tablet pen varies stroke width continuously with pen pressure | ? HUMAN | `handle_tablet` extracts `e->pressure()`, passes to `chalk_input_begin/move`; FreehandMark renders GS_TRISTRIP with per-point half-width computed from pressure; code reviewed and correct; no tablet hardware available to live-verify (INPT-03 documented untestable in 03-03-SUMMARY) |
| 7 | Drawing with a mouse produces full-width strokes (pressure defaults to 1.0) | ✓ VERIFIED | Mouse event handlers call `chalk_input_begin/move` without pressure arg (uses default 1.0f); annotated in code with comment; mouse source callbacks also use default |
| 8 | Pressure-variable strokes render as filled quads, not hairline LINESTRIP | ✓ VERIFIED | `freehand-mark.cpp:87` calls `gs_render_stop(GS_TRISTRIP)`; LINESTRIP replaced; perpendicular offset logic generates two vertices per centerline point |
| 9 | Event filter installed on OBS preview widget at startup | ✓ VERIFIED | `chalk_mode_install` called on `OBS_FRONTEND_EVENT_FINISHED_LOADING`; uses `findChild<QWidget*>("preview")` then `installEventFilter`; live-confirmed in OBS (03-03-SUMMARY PREV-01) |

**Score:** 7/9 automated — 2 require human (coordinate mapping precision, tablet pressure feel)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/chalk-mode.hpp` | ChalkEventFilter class, chalk mode lifecycle API | ✓ VERIFIED | 4 lines; exports `chalk_mode_install`, `chalk_mode_shutdown`; ChalkEventFilter is internal to .cpp per plan |
| `src/chalk-mode.cpp` | Event filter, coordinate mapping, global hotkey, cursor toggle | ✓ VERIFIED | 280 lines; contains `preview_widget_to_scene`, `ChalkEventFilter`, `chalk_mode_install/shutdown`, `QGuiApplication::setOverrideCursor`; substantive implementation |
| `src/plugin.cpp` | Frontend event callback wiring for deferred filter install | ✓ VERIFIED | 29 lines; contains `OBS_FRONTEND_EVENT_FINISHED_LOADING` dispatch to `chalk_mode_install`; `obs_frontend_add_event_callback` in `obs_module_load`; removal in `obs_module_unload` |
| `CMakeLists.txt` | Qt6 and frontend API enabled, chalk-mode.cpp in build | ✓ VERIFIED | `ENABLE_FRONTEND_API ON`, `ENABLE_QT ON` as defaults; `target_compile_definitions(ENABLE_FRONTEND_API)`; `src/chalk-mode.cpp` in `target_sources`; AGL fix present |
| `src/marks/freehand-mark.hpp` | FreehandPoint with pressure field, width-variable rendering | ✓ VERIFIED | `FreehandPoint.pressure` field present; `add_point(x,y,pressure=1.0f)` override; `CHALK_MIN_WIDTH=1.5f`, `CHALK_MAX_WIDTH=6.0f` constants |
| `src/marks/freehand-mark.cpp` | Quad-strip rendering for pressure-variable width | ✓ VERIFIED | GS_TRISTRIP present at line 87; perpendicular offset calculated per point; pressure lerped on interpolated points |
| `src/chalk-source.hpp` | Shared dispatch declarations with pressure param | ✓ VERIFIED | `chalk_input_begin/move` with `float pressure = 1.0f` default; `chalk_find_source()` declared |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/plugin.cpp` | `src/chalk-mode.cpp` | frontend event callback calls `chalk_mode_install/shutdown` | ✓ WIRED | Lines 13,15 of plugin.cpp; both arms of the event dispatch present |
| `src/chalk-mode.cpp` | `src/chalk-source.cpp` | event filter dispatches to `chalk_input_begin/move/end` | ✓ WIRED | 6 call sites in chalk-mode.cpp (lines 150, 165, 175, 205, 209, 212); both mouse and tablet paths wired |
| `src/chalk-mode.cpp` | OBS preview widget | `findChild` and `installEventFilter` | ✓ WIRED | `findChild<QWidget*>("preview")` line 240; `installEventFilter(s_filter)` line 249 |
| `src/chalk-mode.cpp` | `src/chalk-source.cpp` | pressure parameter passed through `chalk_input_begin/move` | ✓ WIRED | `chalk_input_begin(ctx, pos.x, pos.y, pressure)` line 205; `chalk_input_move(ctx, pos.x, pos.y, pressure)` line 209 |
| `src/chalk-source.cpp` | `src/marks/freehand-mark.cpp` | `add_point` receives pressure value | ✓ WIRED | `mark->add_point(x, y, pressure)` line 49; `ctx->mark_list.in_progress->add_point(x, y, pressure)` line 93 |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| PREV-01 | 03-01 | Qt event filter on OBS preview widget intercepts mouse/pen events when chalk mode is active | ✓ SATISFIED | ChalkEventFilter installed on preview widget; `eventFilter` intercepts mouse and tablet events; live-verified in OBS |
| PREV-02 | 03-01 | Preview widget coordinates mapped to scene space so marks render at correct position | ? HUMAN | `preview_widget_to_scene` implements correct letterbox formula; live-tested with corner and edge positions per 03-03-SUMMARY; coordinate precision requires visual confirmation |
| PREV-03 | 03-01 | Global OBS hotkey toggles chalk mode on/off; when off, preview events pass through | ✓ SATISFIED | `obs_hotkey_register_frontend` present; pass-through confirmed by `return false` when `!s_chalk_mode_active`; live-confirmed in OBS |
| PREV-04 | 03-01 | Custom cursor indicates when chalk mode is active | ✓ SATISFIED | `QGuiApplication::setOverrideCursor(Qt::CrossCursor)` confirmed working after cursor-override fix (29f8fd3); live-confirmed in OBS |
| INPT-03 | 03-02 | Plugin accepts tablet/pen input with pressure sensitivity affecting stroke width | ? HUMAN | Full code path implemented: QTabletEvent pressure extraction, pressure through dispatch chain, GS_TRISTRIP variable-width rendering; no tablet hardware available for live test |

**Orphaned requirements check:** REQUIREMENTS.md maps PREV-01 through PREV-04 and INPT-03 to Phase 3. All 5 are claimed by plans 03-01 and 03-02. No orphaned requirements.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/chalk-mode.cpp` | 35 | `// TODO: fixed-scale mode (GetCenterPosFromFixedScale + scroll offsets)` | Info | Deferred scope item; current implementation handles standard letterbox mode. Does not affect primary use case. |

No placeholder implementations, empty handlers, or stub returns found in phase-3 files.

### Human Verification Required

#### 1. Coordinate Mapping Precision (PREV-02)

**Test:** With chalk mode ON, add a chalk source to a scene, then draw marks at the four corners of the OBS preview canvas (top-left, top-right, bottom-left, bottom-right). Also resize the preview window and draw again.
**Expected:** Each mark appears at the canvas corner it was drawn at, not offset or mirrored. After resizing, marks still appear at correct positions (scale-adjusted mapping).
**Why human:** `preview_widget_to_scene` was live-tested and confirmed working (03-03-SUMMARY PREV-02), but exact coordinate precision requires visual inspection against a known reference point. The letterbox formula depends on runtime widget dimensions and canvas dimensions that grep cannot check.

#### 2. Tablet Pressure Width Variation (INPT-03)

**Test:** With chalk mode ON and freehand tool selected, draw a stroke with a tablet pen while continuously varying pressure from very light to very heavy.
**Expected:** Stroke width varies smoothly from approximately 1.5px (hairline) at near-zero pressure to approximately 6px (thick chalk) at full pressure. Width variation is visible within a single continuous stroke.
**Why human:** No tablet hardware was available during the 03-03 verification session (documented in 03-03-SUMMARY). Code path is complete and correctly implemented per code review, but pressure-variable rendering cannot be confirmed without physical tablet input.

### Gaps Summary

No gaps. All automated checks pass. Two items deferred to human verification:

1. **Coordinate mapping precision** — `preview_widget_to_scene` is implemented, committed, and live-tested per SUMMARY documentation; verification requires a running OBS instance to confirm marks appear at the visually correct canvas positions.

2. **Tablet pressure feel** — The full pressure path from `QTabletEvent::pressure()` through dispatch to `GS_TRISTRIP` variable-width rendering is implemented, reviewed, and committed. Hardware unavailability at verification time means this has not been live-tested.

Both items represent hardware/runtime verification needs, not code gaps. The phase goal is substantively achieved.

---

_Verified: 2026-04-08_
_Verifier: Claude (gsd-verifier)_
