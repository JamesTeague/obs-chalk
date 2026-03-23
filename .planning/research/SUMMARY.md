# Project Research Summary

**Project:** obs-chalk
**Domain:** Native C++ OBS Studio plugin — drawing overlay (telestrator) for sports film review
**Researched:** 2026-03-23
**Confidence:** MEDIUM-HIGH

## Executive Summary

obs-chalk is a native OBS Studio plugin implementing a drawing overlay (telestrator) for football film review on livestream. The correct implementation is a C++17 input source (`OBS_SOURCE_TYPE_INPUT`) built against libobs and Qt6, registered with OBS's plugin system via obs-plugintemplate's CMake scaffolding. The plugin maintains a vector object model of marks (a `std::vector` of polymorphic mark objects) that is rendered fresh each frame via `video_render()` using gs_draw primitives. This is a deliberate departure from the existing reference implementation, obs-draw, which paints into a persistent GPU texture and crashes on clear due to a render-thread/main-thread GPU resource race.

The recommended architecture is straightforward: an abstract `Mark` base class with concrete subclasses per tool type (freehand, arrow, circle, cone), a mutex-guarded `MarkList` as the shared state boundary between the render thread and all writers, a `ChalkDockPanel` (QDockWidget) for tool selection that communicates only via Qt-safe mechanisms, and an `HotkeyManager` that registers all primary actions as OBS hotkeys (making Stream Deck integration free). The build order flows from the mark type hierarchy upward through the render loop, then hotkeys, then the Qt dock — allowing the core drawing to be verified before any Qt work begins.

The primary technical risks are all threading-related: calling gs_* functions outside the graphics context (the root cause of obs-draw's crash), nesting obs_enter_graphics (causes deadlock), and touching Qt widgets from the render thread (causes non-deterministic crashes). These risks are not subtle — they are the known failure modes of existing OBS drawing plugins. Following the object model approach and keeping GPU work strictly inside `video_render()` eliminates all three.

## Key Findings

### Recommended Stack

The entire stack flows from one constraint: the plugin must link against the exact Qt6 version bundled with OBS (Qt 6.8.3 in OBS 32.x). Homebrew's Qt 6.9.x cannot be mixed with OBS's Qt 6.8.3 — the plugin will load and then crash. The correct macOS build path links `CMAKE_PREFIX_PATH` against `/Applications/OBS.app/Contents/Frameworks` directly. CMake 3.28–3.30 is required by obs-plugintemplate (3.31+ is outside the template's stated range). The obs-plugintemplate is the mandatory starting point — it provides CMake presets, GitHub Actions CI for all three platforms, and clang-format integration.

**Core technologies:**
- C++17: plugin implementation language — required by Qt6; libobs is C99 but mixing is standard practice
- CMake 3.28–3.30: the only supported OBS plugin build system — obs-plugintemplate's upper bound is 3.30
- libobs 31.x/32.x: provides video_render, mouse interaction callbacks, gs_* drawing primitives, hotkey API
- obs-frontend-api: dock registration (`obs_frontend_add_dock_by_id()`), scene event callbacks — separate from libobs
- Qt 6.8.3 (must match OBS binary exactly): QDockWidget dock panel, QTabletEvent pressure input
- obs-plugintemplate: CMake scaffolding, CI — not optional for a greenfield plugin
- gs_render_start/stop / gs_vertex2f: immediate-mode vector drawing — the correct primitive for the object model
- Universal binary (arm64 + x86_64): required for macOS distribution — OBS.app is universal

**What to avoid:**
- Qt5 (dropped in OBS 28+, will not load in OBS 32)
- Homebrew Qt6 linked against OBS.app (version mismatch crashes)
- obs-draw's pixel/texture painting architecture (the crash cause this project exists to fix)
- `obs_frontend_add_dock()` (deprecated since OBS 30.0)
- CMake below 3.28

### Expected Features

The feature landscape is well-defined. Every competitor (obs-draw, obs-whiteboard, Instant Highlight Draw) covers freehand and basic shapes but none has a cone of vision tool, laser pointer, or hotkey-driven color cycling. obs-chalk's football-specific differentiation is genuine and unoccupied.

**Must have (table stakes):**
- Object model rendering — foundational; everything else depends on it being crash-free
- Freehand drawing — every telestrator has it; missing = product feels broken
- Arrow tool — universal pointing tool; obs-draw doesn't have it yet, notable gap
- Circle tool — circling players/gaps is the most common coaching annotation
- Clear all marks (hotkey primary) — without a hotkey this is unusable during a stream
- Undo last mark — expected by anyone who has used any drawing tool
- Quick color cycling (hotkey-driven palette, 4-6 colors) — obs-draw requires a dialog; this is the UX differentiator
- OBS hotkey integration for all primary actions — required for streaming workflow
- Qt dock panel for tool selection — the OBS plugin pattern users expect
- Stability (no crash) — the entire reason this project exists over obs-draw

**Should have (competitive differentiators):**
- Cone of vision tool — unique in the OBS ecosystem; validates the football-specific direction
- Laser pointer (toggle-hold, ephemeral) — unique; covers ephemeral pointing without polluting mark history
- Pick-to-delete mode — no OBS drawing plugin has this; feasible only because of the object model
- Tablet/pen pressure sensitivity — add after core mouse path is solid
- Optional clear-on-scene-transition — off by default; low-effort QoL for multi-scene setups

**Defer (v2+):**
- Save/load mark sets — adds file I/O and serialization for a workflow that is inherently ephemeral
- Text/label annotations — interrupts stream flow and high complexity for the "pause, draw, talk, clear" cycle
- Animated/sequenced drawings — video editing territory, not a livestream overlay feature
- Recording/replay of drawing sequence — separate product problem

### Architecture Approach

The architecture has two distinct thread domains that share one mutex-protected data structure. The render thread owns `video_render()` and all gs_* operations; it reads the mark list each frame and draws every committed mark plus any in-progress mark. The UI/main thread owns all writes: mouse event callbacks update the in-progress mark, hotkey callbacks mutate the committed list, and Qt dock actions update tool state. All mark list access — reads and writes — goes through a `std::mutex` on `MarkList`. The dock communicates back to the source exclusively via Qt-safe mechanisms (no direct Qt widget calls from render callbacks).

**Major components:**
1. `plugin.cpp` — thin module entry point: registers source type, adds dock, wires frontend event callbacks
2. `ChalkSource` — OBS source object; owns the render loop and receives mouse/key input from OBS preview
3. `MarkList` — shared mutable state; `std::vector<unique_ptr<Mark>>` + in-progress slot + `std::mutex`
4. `Mark` (abstract) + subclasses — one concrete subclass per tool type; each owns its `draw()` and `hit_test()` methods
5. `ToolState` — active tool and color; written by UI thread via atomics, read by render thread
6. `ChalkDockPanel` — QDockWidget for tool selection; communicates to source only via Qt::QueuedConnection
7. `HotkeyManager` — registers and dispatches all OBS hotkeys; always mutates mark list under lock

**Recommended project structure:** flat `src/` with a `marks/` subdirectory (one file per mark type) and a `dock/` subdirectory (Qt code isolated from libobs headers). This enforces the Qt/libobs header separation as a build-time constraint, not just a convention.

**Suggested build order:** mark.hpp → concrete mark types → MarkList → ToolState → ChalkSource → HotkeyManager → ChalkDockPanel → plugin.cpp. This lets drawing be verified in the OBS preview before any Qt work begins.

### Critical Pitfalls

1. **Calling gs_* outside the graphics context** — the root cause of obs-draw's crash. Every GPU call must be inside `video_render()` or an explicit `obs_enter_graphics/leave_graphics` block. Hotkey callbacks and Qt slots must never touch gs_*. The object model makes this natural: those callbacks write to the mark list (CPU), `video_render` reads and draws (GPU).

2. **Nested obs_enter_graphics — deadlock** — obs-draw has this exact pattern (`draw_clear` calls `copy_to_undo` which calls `obs_enter_graphics`, then calls it again). `video_render` already holds the graphics context; never call `obs_enter_graphics` inside it. Single-level entry only.

3. **Modifying the mark list without thread protection** — the render thread reads every frame while hotkey, mouse, and Qt callbacks write from other threads. Every access path — read and write — must hold the `MarkList` mutex. No exceptions; the "it's fast enough" shortcut causes intermittent crashes that are impossible to reproduce in testing.

4. **Qt UI operations from non-Qt threads** — `video_render`, hotkey callbacks, and mouse event handlers all run on non-Qt threads. Any direct Qt widget call from these contexts crashes non-deterministically. All dock updates must use `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.

5. **Wrong source type** — a filter plugin (`OBS_SOURCE_TYPE_FILTER`) cannot be independently positioned or receive mouse input as an independent overlay. obs-chalk must be `OBS_SOURCE_TYPE_INPUT` with `OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_CUSTOM_DRAW`. Establish this in the first commit; it never changes.

## Implications for Roadmap

Based on combined research, the dependency graph is clear: the object model is foundational to all other features, all threading rules must be established before any GPU code exists, and the Qt dock is additive on top of a working render loop. This maps cleanly to 4 phases.

### Phase 1: Plugin Scaffold and Core Rendering

**Rationale:** The object model architecture must exist before any feature work. Threading rules established here are invariants that cannot be retrofitted. The source type decision (INPUT vs FILTER) must be correct from the first commit. This phase's output is a plugin that loads into OBS, renders a transparent overlay, and accepts mouse input — no tools yet.

**Delivers:** Working OBS plugin binary; ChalkSource registered as INPUT source; MarkList with mutex; abstract Mark base class; one concrete mark type (freehand) proving the render loop; source visible in OBS preview with correct transparency and canvas dimensions.

**Addresses:** Stability (table stakes), overlay renders into stream output (table stakes), object model architecture (foundational differentiator)

**Avoids:** GPU functions outside graphics context (Pitfall 1), nested obs_enter_graphics (Pitfall 2), mark list without thread protection (Pitfall 4), wrong source type (Pitfall 7)

**Research flag:** Standard patterns — OBS source registration is well-documented in official docs. No additional research needed.

### Phase 2: Drawing Tools and Hotkeys

**Rationale:** With the render loop and object model solid, add all drawing tools as mark subclasses and wire all primary actions to OBS hotkeys. Tools and hotkeys are developed together because hotkey registration happens in source `create` and the features are interdependent (e.g., undo hotkey requires MarkList::undo_last).

**Delivers:** Arrow, circle, cone of vision, and laser pointer tools; undo, clear-all, and color cycling via hotkeys; pick-to-delete as a mode; quick color palette (4-6 presets). Stream Deck integration is free at this point — it's a consequence of correct hotkey registration.

**Addresses:** All P1 features (arrow, circle, cone, laser, undo, clear, color cycling, hotkeys), pick-to-delete (P2)

**Avoids:** Hotkey registration in module_load instead of source create (integration gotcha), hotkey persistence across OBS restart (requires obs_hotkey_save/load in settings callbacks)

**Research flag:** Cone of vision geometry (hit-testing a cone wedge) may need a brief spike. Hotkey API is well-documented. No phase-level research needed.

### Phase 3: Qt Dock Panel

**Rationale:** The dock is additive UI that mirrors what hotkeys already do. Building it after Phase 2 means all state mutations are already correct; the dock only needs to trigger them safely. Qt/libobs thread boundary violations are most likely to appear here — isolating this phase makes them easy to catch.

**Delivers:** ChalkDockPanel (QDockWidget) with tool selection buttons, color swatch palette, clear button; proper Qt::QueuedConnection for all cross-thread communication; dock persists across OBS sessions via `obs_frontend_add_dock_by_id()`.

**Addresses:** Qt dock panel (table stakes), tablet/pen pressure sensitivity (P2 — QTabletEvent lives in the dock's draw panel)

**Avoids:** Qt UI from non-Qt threads (Pitfall 5), OBSQTDisplay destruction race (Pitfall 6 — architectural decision: no embedded preview in dock), `obs_frontend_add_dock()` deprecated API

**Research flag:** OBSQTDisplay destruction race is a known pitfall. Decision is already made (don't embed a preview), so no additional research needed. Qt dock integration has established patterns from obs-draw reference.

### Phase 4: Polish and Distribution

**Rationale:** After core functionality is stable and serving real streams, address QoL features and macOS distribution requirements. Distribution has a long tail (codesigning, notarization) that can block real users — do it before any public release.

**Delivers:** Optional clear-on-scene-transition (off by default); hotkey persistence verification; universal binary (arm64 + x86_64); macOS codesigning and notarization via obs-plugintemplate CI; "looks done but isn't" checklist verification.

**Addresses:** Clear-on-scene-transition (P2), stroke width per-tool if validated (P3), macOS distribution

**Avoids:** macOS codesigning surprises (requires Developer ID certs configured in CI before first public release), frontend event callback not unregistered in ds_destroy (callback fires on destroyed source)

**Research flag:** macOS codesigning and notarization via obs-plugintemplate CI/CD. The template has this built in, but cert configuration and bundleId setup need attention before first public release. Brief research spike recommended.

### Phase Ordering Rationale

- Phase 1 before everything: GPU threading rules are invariants. Retrofitting them is expensive and risky. The object model's correctness is the project's entire value proposition.
- Phase 2 before Phase 3: Hotkeys establish all state mutation paths. The dock is a UI skin on those paths. Building the dock before the mutation paths creates coupling in the wrong direction.
- Phase 3 before Phase 4: Polish and distribution depend on a feature-complete, stable plugin. Distribution in particular should not be attempted while core features are still in flux.
- The entire ordering avoids the obs-draw mistake: that plugin's architecture choices created a class of bugs that cannot be safely patched. Correctness first, features second, distribution third.

### Research Flags

Phases needing deeper research during planning:
- **Phase 4:** macOS codesigning and notarization via obs-plugintemplate CI. Template supports it, but Developer ID cert configuration and bundleId requirements need a spike before the first public release attempt.

Phases with standard patterns (skip research-phase):
- **Phase 1:** OBS source registration, obs_source_info callbacks, gs_* primitives — all covered in official docs at HIGH confidence. The object model pattern is well-understood.
- **Phase 2:** All drawing tools are straightforward geometry. OBS hotkey API is well-documented. Cone hit-testing is a small geometry problem, not a research problem.
- **Phase 3:** Qt dock integration follows the obs-draw reference implementation pattern. The threading rules (QueuedConnection) are established best practice.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | MEDIUM-HIGH | Core stack confirmed via official docs and obs-draw source. Qt version pinning (6.8.3 vs 6.9.x) requires verification against installed OBS version. |
| Features | MEDIUM-HIGH | Competitor analysis from direct source inspection (obs-draw) and product research. Some anti-feature exclusions are inference from the target workflow rather than validated user research. |
| Architecture | MEDIUM-HIGH | Core patterns (object model, mutex strategy, thread separation) are HIGH confidence from official docs. OBS threading guarantees between UI/hotkey/render callbacks are underdocumented — treat all callbacks as potentially concurrent. |
| Pitfalls | HIGH | GPU threading pitfalls confirmed via direct obs-draw source analysis and OBS issue tracker. Qt threading pitfalls are well-documented. macOS distribution specifics are MEDIUM. |

**Overall confidence:** MEDIUM-HIGH

### Gaps to Address

- **OBS threading guarantees:** The exact thread on which hotkey callbacks fire is not formally documented. The mitigation (always lock the mark list) is correct regardless, but the precise threading model should be confirmed by reading OBS source or community docs during Phase 1 implementation.
- **Qt 6.8.3 exact version match:** Verified from OBS 32.0 release notes (via secondary source). Confirm the exact Qt subversion by inspecting `/Applications/OBS.app/Contents/Frameworks` on the development machine before writing any CMakeLists.txt.
- **Cone of vision hit-testing:** Not researched in depth. Cone geometry hit-testing (point-in-wedge) is a solved problem, but the exact approach (angle comparison vs cross-product) should be decided during Phase 2 mark implementation.
- **Tablet input path:** QTabletEvent on macOS requires the draw target to be a QWidget that receives tablet events. Whether OBS's preview widget forwards these or whether the dock's own panel must be the draw target needs to be confirmed during Phase 3.

## Sources

### Primary (HIGH confidence)
- [OBS Plugin Template — GitHub](https://github.com/obsproject/obs-plugintemplate) — CMake version bounds, Qt6 requirement, Xcode 16 / macOS 14.5 build host
- [OBS Studio Plugins docs](https://docs.obsproject.com/plugins) — plugin type registration, OBS_DECLARE_MODULE() pattern
- [Core Graphics API](https://docs.obsproject.com/reference-libobs-graphics-graphics) — gs_draw_mode enum, gs_vertex2f, gs_render_start/stop, gs_texrender_*
- [Source API Reference](https://docs.obsproject.com/reference-sources) — obs_source_info callbacks, interaction flags, lifecycle
- [OBS Frontend API](https://docs.obsproject.com/reference-frontend-api) — obs_frontend_add_dock_by_id, event callbacks, hotkeys
- [libobs/graphics/graphics.h — GitHub](https://github.com/obsproject/obs-studio/blob/master/libobs/graphics/graphics.h) — authoritative function signatures
- [OBS Backend Design](https://docs.obsproject.com/backend-design) — threading model, pipeline architecture
- obs-draw source code (`draw-source.c`) — direct analysis confirming GPU race condition in draw_clear / copy_to_undo

### Secondary (MEDIUM confidence)
- [OBS Studio 32.1.0 release](https://github.com/obsproject/obs-studio/releases) — latest stable confirmed
- [OBS 32.0 release notes](https://obsproject.com/blog/obs-studio-32-0-release-notes) — Qt 6.8 and plugin version enforcement
- [OBS Thread Safety Discussion #5556](https://github.com/obsproject/obs-studio/discussions/5556) — explicit acknowledgment of underdocumented thread safety
- [OBS PR #4576](https://github.com/obsproject/obs-studio/pull/4576) — OBSQTDisplay destruction race (GLXBadDrawable pattern)
- [OBS Forum — obs-draw resource](https://obsproject.com/forum/resources/draw.2081/) — user feedback and feature requests
- [obs-stabilizer macOS build discovery](https://github.com/azumag/obs-stabilizer) — OBS.app framework path discovery pattern

### Tertiary (LOW confidence)
- [KlipDraw](https://www.klipdraw.com/en/), [FingerWorks Telestrators](https://telestrator.com/), [Chyron PAINT](https://chyron.com/) — professional broadcast telestrator feature reference (marketing copy, not verified documentation)
- [Kinovea annotation tools](https://kinovea.readthedocs.io/en/latest/annotation/general_tools.html) — sports annotation feature landscape reference

---
*Research completed: 2026-03-23*
*Ready for roadmap: yes*
