# Phase 5: Status Indicator and Distribution — Discussion Context

## Decisions

### Status indicator dock panel (not full toolbar)
- OBS dock panel (Qt widget) that shows current state: active tool, selected color, chalk mode on/off
- Read-only for v1 — displays state, not clickable for tool/color selection
- Dockable: user positions it wherever they want in OBS layout
- Not part of the stream output — viewers don't see it
- Serves the streaming use case: stream deck sends hotkey commands, dock shows what state you're in

### macOS codesigned build (DIST-01)
- Existing requirement, no changes from original scope

### Windows x64 build (DIST-02)
- Existing requirement, no changes from original scope
- MVP must be usable on the streaming machine (Windows)

## AI Discretion

- Dock panel layout and visual design
- How state updates propagate from ChalkSource to the dock widget (signal/slot, polling, etc.)
- Whether dock shows delete mode and chalk mode as separate indicators or combined

## Deferred

- Full clickable toolbar with tool/color buttons: v1.1
- Stream deck integration via OBS WebSocket protocol: future
