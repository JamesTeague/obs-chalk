# Feature Research

**Domain:** OBS drawing/telestrator overlay plugin — sports/football film review on livestream
**Researched:** 2026-03-23
**Confidence:** MEDIUM-HIGH — primary source is competitor product research (obs-draw, Kinovea, KlipDraw, FingerWorks, Chyron, professional broadcast tools) + OBS forum user feedback. Some claimed features are inferred from marketing copy rather than verified documentation.

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume any drawing overlay has. Missing one of these means the product feels broken or half-baked.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Freehand drawing | The most basic annotation capability — every telestrator in existence has it | LOW | Needs smooth path smoothing to not look jagged at lower framerates |
| Arrow tool | Standard in every telestrator from ProPresenter to FingerWorks to obs-draw — users point at things | LOW | Click-drag. Arrow end styles vary but at least one arrow style is expected |
| Circle/ellipse tool | Universal tool for circling players, highlighting gaps in coverage | LOW | Click-drag radius. Filled and unfilled variants are nice but unfilled is sufficient |
| Clear all marks | Every tool has it. Users clear at play transitions. Missing = product is unusable in practice | LOW | Hotkey-driven is expected (not just a button) — obs-draw forum explicitly requested this |
| Undo last mark | Every drawing tool has undo. Missing = frustrating for correction mid-stream | LOW | Single-level undo is acceptable. Multi-level is nice |
| Color selection | Every telestrator has multiple colors. Red/yellow/white are the broadcast standard | LOW | At minimum a few preset colors. Quick-switch matters more than a color picker |
| OBS hotkey integration | Streamers operate with keyboard or stream deck. If actions require clicking the dock, it's unusable during a stream | MEDIUM | OBS's native hotkey API (`obs_hotkey_register_source`) must be used — this is what obs-draw does and what users expect |
| Qt dock panel for tool selection | OBS plugins expose UI via Qt docks. Users expect a configuration panel inside OBS | MEDIUM | Dock is setup/config; hotkeys are the streaming runtime interface |
| Overlay renders into stream output | The drawing must appear in the stream, not just in the OBS preview. This is the whole point | HIGH | OBS source plugin that composites over video. obs-draw uses a filter approach; a source approach is also valid |
| Works without crashing OBS | Implicit table stakes — but worth calling out explicitly because obs-draw fails here. Stability is load-bearing for broadcaster trust | HIGH | This is the core motivator for obs-chalk. The object model architecture exists to eliminate the GPU resource race that crashes obs-draw |

### Differentiators (Competitive Advantage)

Features that separate obs-chalk from generic drawing plugins. These align with the football film review use case that no existing OBS plugin addresses.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Cone of vision tool | No OBS drawing plugin has this. It directly models a concept coaches use constantly — "this safety has to cover this cone." Football-specific and high-signal for the target audience | MEDIUM | Click to place apex (player position), drag to set direction/width, release to commit. Unique to obs-chalk in the OBS ecosystem |
| Laser pointer (toggle-hold, ephemeral) | A laser pointer mode exists in some professional broadcast systems (FingerWorks mentions it) but not in any OBS plugin found. Ephemeral — follows cursor, vanishes on key release, never added to mark list | LOW | Hold key to show, release to hide. Matches natural pointing behavior. Clean because it's never a persistent mark |
| Pick-to-delete mode | No OBS drawing plugin offers targeted mark deletion. obs-draw, obs-whiteboard, Instant Highlight — all do clear-all only. This matters mid-review when one mark is wrong but others are good | MEDIUM | Enter pick mode, click a specific mark to remove it. Requires hit-testing on the object model. Object model architecture is what makes this feasible |
| Object model architecture (no pixel painting) | Directly solves obs-draw's crash mode. Enables undo, pick-to-delete, and clear-all without GPU resource races. The stability difference will be apparent immediately to anyone who has crashed obs-draw | HIGH | This is the core architectural differentiator. Users won't call it "object model" but they'll feel it: "this one doesn't crash" |
| Hotkey-driven color cycling | obs-draw requires opening a color picker. For film review you want to switch from red to yellow in a keystroke, not click through a dialog. Color-per-team or color-per-concept is a common coaching workflow | LOW | Cycle through a small preset palette (4-6 colors) via a single hotkey. Colors are fixed and opinionated, not a picker |
| Stream Deck integration (via OBS hotkey system) | Any action exposed as an OBS hotkey is automatically mappable to a stream deck button. No extra work. Coaches often run stream decks for scene switching — drawing tools living in the same control surface is valuable | LOW-MEDIUM | Falls out of proper OBS hotkey registration. Not a separate feature — it's a consequence of doing hotkeys right |
| Optional clear-on-scene-transition | Quality-of-life for multi-scene setups. Film review often stays in one scene, but transition-clear is expected by users who have seen it requested for obs-draw | LOW | Off by default. One OBS signal hook |
| Football-specific focus | A plugin that says "built for football film review" is immediately more trusted by coaches than a generic drawing tool. The cone of vision tool, the name "obs-chalk", the sports vocabulary in the README — all signal domain understanding | LOW (marketing) | Discoverability via "telestrator"/"telestrate" keywords in plugin metadata. The tool's scope is an explicit differentiator |
| Tablet/pen pressure sensitivity | obs-draw added pressure support (v0.2.1) and it's requested in OBS forums. For coaches using iPads or Wacom tablets, pressure-sensitive freehand drawing feels significantly better | MEDIUM | OBS has tablet input events. Design for it from the start even if day-one is mouse-only. Pressure affects stroke width |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem desirable but create complexity without proportional value for the target workflow.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Text/label annotations | Coaches want to label players, routes, formations | For a livestream tool, text input interrupts the stream flow — you stop talking to type. Text labels also look different across fonts, sizes, and layouts. High implementation complexity for low actual use in the "pause, draw, talk, clear" cycle | Number/letter stamps as preset shapes are a v2 idea if user demand validates it |
| Marks tracking with video movement | Professional telestrators like Chyron PAINT and KlipDraw do motion-tracking annotation. Impressive to watch | Requires video frame analysis, completely outside OBS plugin scope without a dedicated video processing pipeline. The stated workflow is screen-space overlay on paused/slow film — tracking is not needed and not feasible in this architecture | Screen-space overlay on paused film is the workflow. Don't build tracking |
| Save/load mark sets | Useful for play diagramming — save a route tree, reload next week | Adds file I/O, serialization, a persistence model, and UI for library management. The coaching workflow is ephemeral: draw during the stream, clear, done. Static play diagrams belong in playbook software, not a livestream overlay | Deferred. If demand is strong post-launch, add it then |
| Animated/sequenced drawings | KlipDraw and some professional tools support animated drawing sequences — draw routes that animate on screen | Substantial complexity: keyframes, timeline, playback controls. This is video editing software territory. The livestream workflow is real-time pointing and clearing, not animated content production | Not for v1. If obs-chalk becomes a content creation tool post-launch, revisit |
| Fade/timer-based auto-clear per mark | Requested in obs-draw forum. Marks that auto-disappear after N seconds | Complex timing coordination across the mark list. Each mark needs an independent timer. Fades require per-frame opacity calculation. For sports film review, the coach controls the pace — marks should stay until deliberately cleared | Laser pointer covers the ephemeral/pointing use case. Persistent marks + explicit clear-all covers the annotation use case |
| Multi-source support | Generic request for "multiple independent drawing layers" | Multiplies UI/UX complexity significantly. The single-overlay-per-scene model is sufficient for all identified use cases and keeps the architecture simple | One overlay per scene. If a layout needs separate annotation layers, use two OBS scenes |
| Color picker / custom colors | Power users want full color freedom | A color picker dialog is not usable mid-stream. Custom color hex input is dead weight for a coach doing film review. The value is in fast switching, not infinite choices | A curated preset palette of 5-6 high-contrast colors covers all real coaching use cases |
| Recording/replay of drawing sequence | "Content creators could replay their annotations" — requested as a future obs-draw feature | Requires capturing a draw event log and building a playback engine. This is a separate product problem, not a livestream overlay problem | Out of scope. Interesting v2+ idea if obs-chalk gains adoption in content creation |

---

## Feature Dependencies

```
Object Model Architecture
    └──enables──> Undo (pop last object from render list)
    └──enables──> Pick-to-Delete (remove object by reference)
    └──enables──> Clear-All (empty render list)
    └──enables──> Stability (no GPU texture resource races)

OBS Hotkey Registration (obs_hotkey_register_source)
    └──enables──> Stream Deck Integration (free via OBS mapping)
    └──enables──> Color Cycling (hotkey-driven)
    └──enables──> Tool Switching (hotkey-driven)
    └──enables──> Laser Toggle (hotkey-driven)
    └──enables──> Clear-All Hotkey

Qt Dock Panel
    └──requires──> OBS plugin build system (CMake + Qt6)
    └──supports──> Visual tool selection (backup to hotkeys)
    └──supports──> Color palette display

Freehand Tool
    └──enhances──> Tablet Pressure Sensitivity (variable stroke width)

Laser Pointer
    └──conflicts──> Persistent mark list (laser is never added to render list)
    └──requires──> Hotkey Registration (toggle-hold behavior)

Pick-to-Delete Mode
    └──requires──> Object Model Architecture
    └──requires──> Hit-testing on mark geometry
    └──conflicts──> Drawing Mode (must be a separate mode to avoid accidental marks)

Cone of Vision Tool
    └──requires──> Object Model Architecture (multi-point geometry)
    └──requires──> Mouse event handling (apex placement, drag for direction/width)
```

### Dependency Notes

- **Object model is foundational:** Every other feature advantage (undo, pick-to-delete, stability) flows from this. It must be built first and correctly.
- **OBS hotkeys require OBS source/filter registration:** Can't register hotkeys until the plugin source type is registered with OBS. Hotkey implementation depends on getting the basic plugin scaffolding right.
- **Pick-to-delete conflicts with drawing mode:** Must be explicit mode switching (e.g., a toggle hotkey) to avoid a click to delete accidentally starting a new mark.
- **Tablet pressure enhances freehand only:** Pressure sensitivity applies to stroke width on freehand marks. Arrow and circle tools use it for line thickness at most.

---

## MVP Definition

### Launch With (v1)

Minimum to validate the concept and serve the Teague's Take use case.

- [ ] Object model rendering — the architectural foundation; everything else depends on it
- [ ] Freehand drawing tool — table stakes; without it the plugin is useless
- [ ] Arrow tool — table stakes; coaches point at things constantly
- [ ] Circle tool — table stakes; circling players/gaps is the most common annotation
- [ ] Cone of vision tool — the primary differentiator; validates the football-specific direction
- [ ] Laser pointer (toggle-hold, ephemeral) — differentiator; covers ephemeral pointing without persistent marks
- [ ] Undo last mark — table stakes; mistakes happen mid-stream
- [ ] Clear all marks — table stakes; play transitions require instant clear
- [ ] Quick color cycling (hotkey-driven palette, 4-6 colors) — differentiator over obs-draw; essential for fast workflow
- [ ] OBS hotkey integration for all primary actions — table stakes for streaming UX
- [ ] Qt dock panel for tool selection — table stakes for OBS plugin pattern
- [ ] Stability / no crash — this is the whole reason obs-chalk exists

### Add After Validation (v1.x)

Features to add once the core is working and serving real streams.

- [ ] Pick-to-delete mode — valuable differentiator; defer until object model is proven stable. Requires hit-testing implementation.
- [ ] Tablet/pen pressure sensitivity — requested by users; add once mouse path is solid. OBS tablet events exist.
- [ ] Optional clear-on-scene-transition — low-effort QoL; add when scene management is tested

### Future Consideration (v2+)

Defer until product-market fit is established beyond the original use case.

- [ ] Additional arrow end styles — minor polish; no users blocked without it
- [ ] Stroke width controls per-tool (beyond pressure) — nice to have but palette cycling matters more
- [ ] Save/load mark sets — only needed if static diagram use cases emerge
- [ ] Text/label stamps (preset letters/numbers as shapes) — only if users report the annotation-text gap
- [ ] Recording/replay of drawing sequence — only if content creation use case grows

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Object model architecture | HIGH | HIGH | P1 — foundation |
| Freehand drawing | HIGH | LOW | P1 |
| Arrow tool | HIGH | LOW | P1 |
| Circle tool | HIGH | LOW | P1 |
| Clear all (hotkey) | HIGH | LOW | P1 |
| Undo last mark | HIGH | LOW | P1 |
| OBS hotkey integration | HIGH | MEDIUM | P1 |
| Qt dock panel | MEDIUM | MEDIUM | P1 |
| Laser pointer (toggle-hold) | HIGH | LOW | P1 |
| Cone of vision tool | HIGH | MEDIUM | P1 — primary differentiator |
| Quick color cycling | HIGH | LOW | P1 |
| Pick-to-delete mode | MEDIUM | MEDIUM | P2 |
| Tablet pressure sensitivity | MEDIUM | MEDIUM | P2 |
| Clear-on-scene-transition | LOW | LOW | P2 |
| Stroke width per-tool | LOW | LOW | P3 |
| Save/load mark sets | LOW | HIGH | P3 — defer |
| Text annotations | LOW | HIGH | P3 — out of scope |
| Motion tracking | LOW | VERY HIGH | Excluded |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

---

## Competitor Feature Analysis

| Feature | obs-draw (Exeldro) | obs-whiteboard (Herschel) | Instant Highlight Draw (mmlTools) | obs-chalk (planned) |
|---------|-------------------|--------------------------|----------------------------------|---------------------|
| Architecture | Pixel buffer (GPU texture) | Unknown (Lua-based) | Unknown | Object model (vector list) |
| Freehand | Yes (pencil, brush) | Yes (mouse/touch) | No | Yes |
| Arrow tool | User-requested, not built yet | No | Yes (basic) | Yes |
| Circle tool | Yes (ellipse) | No | Yes (circle) | Yes |
| Cone of vision | No | No | No | Yes — unique |
| Laser pointer | No | No | No | Yes — unique |
| Undo | Yes | No | Yes | Yes |
| Clear all | Yes (button + hotkey) | Yes (hotkey) | Yes | Yes (hotkey primary) |
| Pick-to-delete | No | No | No | Yes (v1.x) |
| Color selection | Yes (color picker dialog) | Unknown | Yes (color picker + opacity) | Yes (hotkey palette cycle) |
| Quick color switch (hotkey) | No (dialog only) | No | No | Yes — differentiator |
| OBS hotkey integration | Partial (show/hide dock) | Unknown | No | Yes (all actions) |
| Tablet pressure | Yes (added v0.2.1) | No | No | Yes (v1.x) |
| Scene transition clear | Yes (optional) | No | No | Yes (optional, off by default) |
| Crash stability | Known crash on clear | Unknown | Unknown | Object model eliminates GPU race |
| Platform | Windows/macOS/Linux | Windows only | Windows only | macOS primary (OBS cross-platform) |
| Football-specific tools | No | No | No | Yes (cone of vision) |
| Open source | Yes (GPL-2.0) | Yes | Yes | Yes |

---

## Sources

- [obs-draw by Exeldro — OBS Forums](https://obsproject.com/forum/resources/draw.2081/)
- [obs-draw user reviews — OBS Forums](https://obsproject.com/forum/resources/draw.2081/reviews)
- [obs-draw GitHub](https://github.com/exeldro/obs-draw)
- [obs-whiteboard by Herschel — GitHub](https://github.com/Herschel/obs-whiteboard)
- [Instant Highlight Source Draw — GitHub](https://github.com/mmlTools/draw-source)
- [WebRTC-Telestrator — OBS Forums](https://obsproject.com/forum/resources/webrtc-telestrator.1824/)
- [Kinovea annotation tools documentation](https://kinovea.readthedocs.io/en/latest/annotation/general_tools.html)
- [KlipDraw telestration for sports](https://www.klipdraw.com/en/)
- [FingerWorks Telestrators](https://telestrator.com/)
- [Chyron PAINT telestrated replay](https://chyron.com/products/all-in-one-production-systems/telestrated-replay-and-sports-analysis/)
- [Sports telestration overview — AnalysisPro](https://www.analysispro.com/sports-telestration)
- [Telestrator — Wikipedia](https://en.wikipedia.org/wiki/Telestrator)

---
*Feature research for: OBS drawing/telestrator overlay — football film review on livestream*
*Researched: 2026-03-23*
