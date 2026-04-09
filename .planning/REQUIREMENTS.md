# Requirements: obs-chalk

**Defined:** 2026-03-23
**Core Value:** Coaches can reliably draw on film during a livestream without the tool crashing or disrupting the broadcast.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### Core

- [x] **CORE-01**: Plugin registers as an OBS input source that composites as a transparent overlay in the scene
- [x] **CORE-02**: Drawing marks are vector objects in a render list, not pixels in a texture buffer
- [x] **CORE-03**: Render list is mutex-protected for thread safety between render thread and input callbacks
- [x] **CORE-04**: All GPU rendering happens exclusively within `video_render` callback (no `obs_enter_graphics` from other threads)
- [x] **CORE-05**: Plugin builds against OBS 31.x+ using obs-plugintemplate and CMake

### Tools

- [x] **TOOL-01**: User can draw freehand strokes with mouse movement
- [x] **TOOL-02**: User can draw arrows by click-drag (multiple end styles available)
- [x] **TOOL-03**: User can draw circles by click-drag to set center and radius
- [x] **TOOL-04**: User can place a cone of vision by clicking to set apex, dragging to set direction and width, and releasing to commit
- [x] **TOOL-05**: User can activate a laser pointer by holding a key (visible while held, disappears on release, never added to mark list)

### Mark Lifecycle

- [x] **MARK-01**: User can undo the most recent mark via hotkey
- [x] **MARK-02**: User can enter pick-to-delete mode, click a specific mark, and remove only that mark
- [x] **MARK-03**: User can clear all marks at once via hotkey
- [x] **MARK-04**: Multi-level undo (user can undo multiple times to step back through mark history)

### Input

- [x] **INPT-01**: All primary actions (tool switch, undo, clear, laser toggle, color cycle) are registered as OBS hotkeys
- [x] **INPT-02**: User can cycle through a preset color palette via hotkey
- [x] **INPT-03**: Plugin accepts tablet/pen input with pressure sensitivity affecting stroke width

### Preview Interaction

- [x] **PREV-01**: Qt event filter on OBS preview widget intercepts mouse/pen events when chalk mode is active
- [x] **PREV-02**: Preview widget coordinates are mapped to scene space so marks render at the correct position
- [x] **PREV-03**: Global OBS hotkey toggles chalk mode on/off; when off, all preview events pass through to normal OBS behavior
- [x] **PREV-04**: Custom cursor indicates when chalk mode is active

### UI (Optional)

- [x] **UI-01**: Qt dock panel in OBS for tool selection and color palette display
- [ ] **UI-02**: Dock panel shows current active tool and selected color

### Integration

- [x] **INTG-01**: Optional clear-on-scene-transition (off by default, configurable in source settings)

### Distribution

- [ ] **DIST-01**: macOS universal binary (Intel + Apple Silicon)
- [ ] **DIST-02**: Windows x64 build

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Tools

- **TOOL-06**: Number/letter stamp shapes for labeling players
- **TOOL-07**: Filled vs unfilled circle variant toggle

### Mark Lifecycle

- **MARK-05**: Save/load mark sets (persist play diagrams)

### Input

- **INPT-04**: Touch screen input support

### Distribution

- **DIST-03**: Published to OBS plugin repository

## Out of Scope

| Feature | Reason |
|---------|--------|
| Marks tracking with video movement | Workflow is screen-space overlay on paused/slow footage; motion tracking requires video frame analysis outside plugin scope |
| Text/label annotations | Text input interrupts stream flow; stamps are a better v2 path if demand validates |
| Animated/sequenced drawings | Video editing territory, not livestream overlay |
| Fade/timer per mark | Laser pointer covers ephemeral pointing; persistent marks should stay until explicitly cleared |
| Color picker dialog | Not usable mid-stream; preset palette with hotkey cycling is the right interaction |
| Multi-source support | Single overlay per scene is sufficient; multiple scenes handle layout needs |
| Recording/replay of drawing sequences | Separate product problem, not a livestream overlay problem |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| CORE-01 | Phase 1 | Complete |
| CORE-02 | Phase 1 | Complete |
| CORE-03 | Phase 1 | Complete |
| CORE-04 | Phase 1 | Complete |
| CORE-05 | Phase 1 | Complete |
| TOOL-01 | Phase 2 | Complete |
| TOOL-02 | Phase 2 | Complete |
| TOOL-03 | Phase 2 | Complete |
| TOOL-04 | Phase 2 | Complete |
| TOOL-05 | Phase 2 | Complete |
| MARK-01 | Phase 2 | Complete |
| MARK-02 | Phase 2 | Complete |
| MARK-03 | Phase 2 | Complete |
| MARK-04 | Phase 2 | Complete |
| INPT-01 | Phase 2 | Complete |
| INPT-02 | Phase 2 | Complete |
| INPT-03 | Phase 3 | Complete |
| PREV-01 | Phase 3 | Complete |
| PREV-02 | Phase 3 | Complete |
| PREV-03 | Phase 3 | Complete |
| PREV-04 | Phase 3 | Complete |
| INTG-01 | Phase 4 | Complete |
| DIST-01 | Phase 4 | Pending |
| DIST-02 | Phase 4 | Pending |
| UI-01 | Phase 5 (optional) | Complete |
| UI-02 | Phase 5 (optional) | Pending |

**Coverage:**
- v1 requirements: 24 total (4 new PREV-*, UI-01/UI-02 moved to optional Phase 5)
- Mapped to phases: 24
- Unmapped: 0 ✓

---
*Requirements defined: 2026-03-23*
*Last updated: 2026-03-23 after roadmap creation*
