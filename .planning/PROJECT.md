# obs-chalk

## What This Is

A native C/C++ OBS Studio plugin that provides a telestrator-style drawing overlay for football film review on livestreams. Drawing tools are purpose-built for coaching analysis — arrows, circles, freehand, cone of vision, laser pointer — with an object model that enables selective mark deletion. Open source.

## Core Value

Coaches can reliably draw on film during a livestream without the tool crashing or disrupting the broadcast.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] Object-model rendering (marks are discrete objects, not pixels)
- [ ] Freehand drawing tool
- [ ] Arrow tool with multiple end styles
- [ ] Circle tool (click-drag)
- [ ] Cone of vision tool (click to place apex, drag to set direction/width, release to commit)
- [ ] Laser pointer tool (toggle-hold, ephemeral, follows cursor)
- [ ] Undo last mark (hotkey-driven)
- [ ] Pick-to-delete mode (select and remove a specific mark)
- [ ] Clear all marks
- [ ] Quick color switching (hotkey-driven palette cycling)
- [ ] OBS hotkey integration for all primary actions (tools, undo, clear, laser, color)
- [ ] Qt dock panel for tool selection and settings
- [ ] Tablet/pen input awareness (pressure sensitivity)
- [ ] Optional clear-on-scene-transition

### Out of Scope

- Marks tracking with video movement — workflow is screen-space overlay on paused/slow footage
- Text/label annotations — not part of current film review workflow
- Save/load mark sets — deferred, not needed for livestream use case
- Multi-source support — single overlay per scene is sufficient
- Custom stamp/image tools — keep the toolkit focused
- Recording/replay of drawing sequences — future content creation feature

## Context

- **Origin:** OBS Draw (Exeldro) plugin crashes OBS during livestreaming when clearing marks. Root cause is a pixel-painting architecture with GPU resource race conditions. obs-chalk is a ground-up replacement, not a fork.
- **Workflow:** Pause film → annotate → talk through it → clear → advance. Sometimes draw on moving film. ~10 marks upper bound per play. Play-based annotation cycle.
- **Input setup:** Mouse + keyboard primary. Stream deck available for OBS hotkeys. Tablet is a goal but not day-one blocker.
- **Audience:** Football coaches and analysts doing film review on stream. Teague's Take podcast is the first user.
- **Discoverability:** "telestrator" and "telestrate" must appear in repo description, GitHub topics, README, and OBS plugin metadata.
- **Skill building:** First C/C++ OBS plugin for Teague. Rust experience transfers (memory management, systems thinking). obs-draw patch is the learning ramp for OBS SDK and build system.

## Constraints

- **Timeline**: Stream-ready by last week of August 2026 (~5 months)
- **Platform**: OBS Studio 31.x+, macOS primary (Teague's streaming setup)
- **Tech stack**: C/C++ with Qt6 (OBS plugin requirement), CMake build system
- **Architecture**: Object model — marks are vector objects in a render list, not pixel buffers. This is the core departure from obs-draw.
- **File size**: ~500 line limit per implementation file (project convention)

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Object model over pixel painting | Enables selective deletion, trivial undo, and avoids GPU resource race conditions that crash obs-draw | — Pending |
| Native C/C++ plugin over browser source | Direct preview mouse interaction without mode switching, native hotkey integration, no browser engine overhead | — Pending |
| Name: obs-chalk | Short, greppable, football identity ("chalk talk"). Telestrate/telestrator as search keywords. | — Pending |
| Laser pointer is toggle-hold | Hold key to show, release to hide. Matches natural pointing behavior. | — Pending |
| Pick-to-delete + undo-last for mark removal | Undo-last for quick corrections, pick-to-delete for targeted removal mid-review. Clear-all for play transitions. | — Pending |
| Open source from start | Purpose-built for a real community (coaching film review). No reason to close it. | — Pending |

---
*Last updated: 2026-03-23 after initialization*
