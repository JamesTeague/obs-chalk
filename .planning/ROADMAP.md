# Roadmap: obs-chalk

## Overview

obs-chalk is a native C++ OBS plugin that replaces obs-draw with a crash-free object-model telestrator for football film review. The build order flows from correctness outward: establish the threading invariants and object model first, layer drawing tools and hotkeys on top, add the Qt dock as additive UI, then close with polish and distribution. Every phase delivers something that can be verified in OBS before the next begins.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Plugin Scaffold and Core Rendering** - OBS plugin loads with a transparent overlay, mutex-protected mark list, and freehand drawing proving the render loop (completed 2026-03-30)
- [x] **Phase 2: Drawing Tools, Mark Lifecycle, and Hotkeys** - All drawing tools functional, mark management (undo, delete, clear) wired, all primary actions registered as OBS hotkeys (completed 2026-03-30)
- [ ] **Phase 3: Preview Interaction** - Qt event filter on OBS preview widget, coordinate mapping, chalk mode hotkey toggle, pen/tablet pressure, custom cursor
- [ ] **Phase 4: Polish and Distribution** - Scene-transition clear, macOS codesigned build, Windows x64 build
- [ ] **Phase 5: Dock Panel** (OPTIONAL) - Qt dock panel with tool selection, color palette display, and active state indicators

## Phase Details

### Phase 1: Plugin Scaffold and Core Rendering
**Goal**: The plugin loads in OBS as a transparent overlay that renders drawn marks without crashing
**Depends on**: Nothing (first phase)
**Requirements**: CORE-01, CORE-02, CORE-03, CORE-04, CORE-05
**Success Criteria** (what must be TRUE):
  1. Plugin appears in OBS "Add Source" list and composites as a transparent overlay in the scene
  2. A freehand stroke drawn with the mouse is visible in OBS preview on the next frame
  3. Clearing all marks results in a clean transparent frame — no crash, no GPU artifacts
  4. All GPU calls happen inside `video_render`; no gs_* calls fire from hotkey or mouse callbacks
  5. Plugin binary builds cleanly against OBS 31.x+ via CMake and loads without error
**Plans:** 2/2 plans complete
Plans:
- [x] 01-01-PLAN.md — Bootstrap build system and plugin source registration as transparent overlay
- [x] 01-02-PLAN.md — Object model (Mark, FreehandMark, MarkList) with mouse interaction and render loop

### Phase 2: Drawing Tools, Mark Lifecycle, and Hotkeys
**Goal**: Users can draw all tool types, manage marks via undo/delete/clear, and control everything via OBS hotkeys
**Depends on**: Phase 1
**Requirements**: TOOL-01, TOOL-02, TOOL-03, TOOL-04, TOOL-05, MARK-01, MARK-02, MARK-03, MARK-04, INPT-01, INPT-02
**Success Criteria** (what must be TRUE):
  1. User can draw freehand strokes, arrows (multiple end styles), circles, and cone of vision marks using mouse
  2. Laser pointer appears while a key is held and disappears on release without adding a mark to the list
  3. User can undo the most recent mark and step back through multiple marks via repeated undo hotkey
  4. User can enter pick-to-delete mode, click a specific mark, and remove only that mark
  5. All primary actions (tool switch, undo, clear-all, laser toggle, color cycle) are registered OBS hotkeys and appear in OBS hotkey settings
**Plans:** 3/3 plans complete
Plans:
- [x] 02-01-PLAN.md — Expand interfaces (Mark base, ToolState, MarkList, ChalkSource) and freehand interpolation
- [x] 02-02-PLAN.md — ArrowMark, CircleMark, and ConeMark implementations
- [x] 02-03-PLAN.md — Hotkeys, tool routing, laser pointer, pick-to-delete, color cycle, and OBS verification

### Phase 3: Preview Interaction
**Goal**: Users can toggle chalk mode via hotkey and draw directly on the OBS preview without source selection or interaction mode
**Depends on**: Phase 2
**Requirements**: PREV-01, PREV-02, PREV-03, PREV-04, INPT-03
**Success Criteria** (what must be TRUE):
  1. With chalk mode active, clicking and dragging on the OBS preview creates marks at the correct scene-space positions
  2. With chalk mode inactive, all preview mouse/pen events pass through to normal OBS behavior (source selection, transform handles)
  3. A global hotkey toggles chalk mode on/off; the preview cursor changes to indicate active state
  4. Drawing with a tablet pen varies stroke width continuously with pen pressure
  5. Existing source interaction callbacks still work as a secondary input path when chalk mode is off
**Plans:** 2/3 plans executed
Plans:
- [x] 03-01-PLAN.md — Event filter on OBS preview with coordinate mapping, chalk mode hotkey, and cursor (completed 2026-04-08)
- [ ] 03-02-PLAN.md — Tablet pressure sensitivity for variable-width freehand strokes
- [ ] 03-03-PLAN.md — OBS verification of all Phase 3 requirements

### Phase 4: Polish and Distribution
**Goal**: The plugin is ready for real stream use on both macOS and Windows with scene-transition clear and platform builds
**Depends on**: Phase 3
**Requirements**: INTG-01, DIST-01, DIST-02
**Success Criteria** (what must be TRUE):
  1. When the optional clear-on-scene-transition setting is enabled, switching scenes removes all marks; with it disabled, marks persist across scene switches
  2. macOS build is codesigned and notarized; plugin installs from a .pkg or .zip without Gatekeeper warnings
  3. Windows x64 build produces a loadable plugin DLL that installs and runs in OBS on the streaming machine
**Plans**: TBD

### Phase 5: Dock Panel (OPTIONAL)
**Goal**: Users who prefer a visual control surface can use a Qt dock panel for tool selection and color palette
**Depends on**: Phase 3
**Requirements**: UI-01, UI-02
**Success Criteria** (what must be TRUE):
  1. Dock panel appears in OBS UI with buttons for each tool and a color swatch palette; dock persists across OBS restarts
  2. Dock panel visually reflects the currently active tool and selected color when either changes (via hotkey or dock click)
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 (optional)

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Plugin Scaffold and Core Rendering | 2/2 | Complete   | 2026-03-30 |
| 2. Drawing Tools, Mark Lifecycle, and Hotkeys | 3/3 | Complete   | 2026-03-30 |
| 3. Preview Interaction | 2/3 | In Progress|  |
| 4. Polish and Distribution | 0/TBD | Not started | - |
| 5. Dock Panel (OPTIONAL) | 0/TBD | Not started | - |
