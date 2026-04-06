# CONTEXT: obs-chalk

## What This Is

A native C/C++ OBS Studio plugin — a telestrator-style drawing overlay built for football film review on livestreams. Open source.

**Name:** obs-chalk
**Discoverability:** "telestrator", "telestrate" must appear in repo description, GitHub topics, README tagline, and OBS plugin metadata so searches for those terms surface this tool.

## Problem

Existing OBS drawing plugins (obs-draw) are pixel painters — they render brush strokes directly to GPU texture buffers. This makes selective deletion impossible, undo fragile (render snapshots), and the architecture prone to GPU resource crashes during livestreaming. The tools are also generic — no football-specific features like cone of vision.

## Design

### Object Model (Not Pixel Painting)

Each mark is a discrete object in a render list with properties (type, color, position, geometry). Rendering draws the list every frame. This makes:
- **Selective deletion** trivial — remove an object from the list
- **Undo** trivial — pop the last object
- **Clear-all** trivial — empty the list
- The render list stays small (~10 marks is a practical upper bound per play)

### Tools

| Tool | Behavior | Persistence |
|------|----------|-------------|
| Freehand | Draw with mouse/tablet movement | Persistent |
| Arrow | Click-drag, multiple end styles (implementation detail) | Persistent |
| Circle | Click-drag to set center and radius | Persistent |
| Cone of Vision | Click to place apex (player), drag to set direction and width, release to commit | Persistent |
| Laser Pointer | Toggle-hold (hold key to show, release to hide), follows cursor | Ephemeral — never added to render list |

### Mark Lifecycle

1. **Draw** — tool creates an object, added to render list on completion (mouse-up/release)
2. **Persists** — rendered every frame as part of the overlay
3. **Remove** via three paths:
   - **Undo** — removes most recent mark (fast, hotkey-driven, for mistakes)
   - **Pick to delete** — enter a selection mode, click a specific mark to remove it (for targeted deletion mid-review)
   - **Clear all** — empties the entire render list (for play transitions)

### Input

- **Primary:** mouse + keyboard
- **Tablet-aware:** design for pressure sensitivity from the start (OBS has tablet input events). Test with mouse first, tablet support is a goal.
- **OBS hotkeys** for tool switching, color cycling, undo, clear, laser toggle. Using OBS's native hotkey system means stream deck integration comes free.
- **Quick color switching** — hotkey-driven palette cycling, not a color picker dialog. Fast enough to use mid-stream.

### Rendering

- Screen-space overlay — marks are pixel coordinates on the source, they do not track with video movement
- Primary workflow: pause film, annotate, talk, clear, advance. Drawing on moving video happens but is secondary.

### Preview Interaction

- **Chalk mode** — global hotkey toggles drawing on/off. When active, mouse/pen events on the OBS preview are intercepted by a Qt event filter and routed to mark creation. When inactive, events pass through to normal OBS behavior.
- Custom cursor indicates chalk mode state.

### UI

- Qt dock panel in OBS for tool selection and settings (optional, Phase 5)
- Hotkeys are the primary interaction method during streaming — the dock is for setup/configuration

### Scene Transitions

- Optional clear-on-scene-transition (off by default). Film review typically stays in one scene but not always.

## Decisions

1. **Object model architecture.** Marks are vector objects, not pixels. This is the core architectural departure from obs-draw and drives most of the benefits.
2. **Native C/C++ OBS plugin.** Direct preview mouse interaction without mode switching. Native hotkey integration. No browser engine overhead. Also a skill-building goal for Teague.
3. **Name is obs-chalk.** Telestrator/telestrate are discoverability keywords, not the name.
4. **Laser pointer is toggle-hold**, not always-on. Hold a key to show, release to hide.
5. **Pick-to-delete mode** for selective mark removal. Combined with most-recent-first undo for quick corrections.
6. **Open source from the start.**

## Discretion

- Arrow end style variations — types and visual design are implementation detail
- Cone of vision angle defaults and adjustable range
- Color palette choices and number of quick-switch colors
- Hit-testing approach for pick-to-delete (bounding box vs. precise shape testing)
- Undo stack depth
- Tablet pressure mapping (what pressure affects — size, opacity, both)
- Specific hotkey defaults

## Deferred

- Marks tracking with video movement (not needed for current workflow)
- Text/label annotations
- Save/load mark sets (e.g., save a play diagram)
- Multi-source support (multiple draw overlays in one scene)
- Custom stamp/image tools
- Recording/replay of drawing sequence for content creation

## Timeline

Must be stream-ready by last week of August 2026. ~5 months from now.

## Relationship to obs-draw

obs-draw gets a targeted crash fix (separate CONTEXT.md in obs-draw repo). obs-chalk is a ground-up replacement scoped to football film review, not a fork.
