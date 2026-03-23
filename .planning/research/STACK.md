# Stack Research

**Domain:** Native OBS Studio plugin — drawing overlay (telestrator)
**Researched:** 2026-03-23
**Confidence:** MEDIUM-HIGH — core stack confirmed via official docs and reference source code; Qt version pinning requires verification against whatever OBS version is deployed

---

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| C++ | C++17 | Plugin implementation language | OBS Studio itself is C++; the frontend API (Qt docks, QTabletEvent, signal handlers) requires C++. C is sufficient for libobs-only code but Qt forces C++17 minimum. C++20 is usable but not needed here. |
| CMake | 3.28+ (3.30.5 on macOS) | Build system | The only supported build system for OBS plugins. obs-plugintemplate hardcodes CMake 3.28–3.30. Don't use 3.31+ until obs-plugintemplate catches up — it sets an explicit upper bound. |
| libobs | 31.x / 32.x | Core OBS plugin API | Provides the source registration, video_render callback, mouse_click/mouse_move interaction callbacks, hotkey API, and the graphics subsystem (gs_*). This IS the plugin SDK — there is no separate SDK installer. You link against the installed OBS app's frameworks. |
| obs-frontend-api | (ships with OBS) | Dock registration, scene events | Exposes `obs_frontend_add_dock_by_id()` (Qt dock integration), scene transition events, and `obs_frontend_add_event_callback()`. Required for the Qt panel and optional-clear-on-transition feature. Separate from libobs. |
| Qt6 | 6.8.x (must match OBS binary exactly) | Dock panel UI, tablet input | OBS 32.x ships Qt 6.8.3. Plugin Qt must match the OBS runtime Qt version exactly — binary incompatibility will silently prevent the plugin from loading. Qt6 provides QTabletEvent (pressure sensitivity), QDockWidget, QToolBar. |
| obs-plugintemplate | latest main | CMake scaffolding, CI | Official OBS project template. Sets up CMake presets, GitHub Actions for Windows/macOS/Linux, clang-format, and libobs/Qt discovery. Starting point — not optional for a greenfield plugin. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| gs_texrender | (libobs built-in) | Off-screen render target | The object-model approach renders the current mark list to a texrender each frame. Avoids the double-buffer corruption that crashes obs-draw. Use `gs_texrender_begin/end` + `gs_texrender_get_texture`. |
| gs_render_start / gs_render_stop | (libobs built-in) | Immediate-mode vector drawing | Draws line strips, points, and triangles using `gs_vertex2f`. This is how arrows, freehand paths, and circles are drawn without textures. Mode enum: `GS_LINES`, `GS_LINESTRIP`, `GS_TRIS`, `GS_TRISTRIP`. |
| obs-websocket API (vendor requests) | (ships with OBS) | Remote tool switching via Stream Deck | obs-draw uses this pattern for vendor request callbacks. Useful if Stream Deck integration is desired beyond OBS hotkeys. Defer to a later phase — not needed for MVP. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| Xcode 16+ | macOS compiler and SDK | Required by obs-plugintemplate CI. Minimum macOS 14.5 to build (not to run). Target macOS 12+ for distribution. |
| CMake Presets | Reproducible configure/build | obs-plugintemplate ships `CMakePresets.json` with `macos-ci`, `windows-ci`, `linux-ci` presets. Use `cmake --preset macos-ci` for clean builds. |
| clang-format | Code formatting | obs-plugintemplate includes `.clang-format`. CI runs a format check before build. Run `clang-format -i` before committing. |
| Homebrew | macOS dependency management | `brew install cmake ninja pkg-config qt@6` gives build tools. NOTE: Homebrew Qt version (currently 6.9.x) will NOT match OBS's bundled Qt (6.8.3). For running against your installed OBS.app, link against `/Applications/OBS.app/Contents/Frameworks` directly. |
| GitHub Actions | CI/CD | obs-plugintemplate workflows build and package for all three platforms automatically. Use from day one — cross-platform breakage discovered late is expensive. |

---

## Installation

The plugin template is not a package — it's a GitHub template repository. Bootstrap:

```bash
# Clone the official template (or use GitHub's "Use this template" button)
gh repo create obs-chalk --template obsproject/obs-plugintemplate --public

# macOS build deps
brew install cmake ninja pkg-config

# DO NOT install qt@6 from Homebrew for linking against installed OBS.app
# Link against OBS.app's bundled Qt instead (see Version Compatibility below)

# Configure and build
cmake --preset macos-ci
cmake --build build_macos
```

For linking against your installed OBS app (macOS):

```cmake
# In CMakeLists.txt, add before find_package(Qt6):
set(CMAKE_PREFIX_PATH "/Applications/OBS.app/Contents/Frameworks;${CMAKE_PREFIX_PATH}")
set(OBS_SDK_DIR "/Applications/OBS.app/Contents/Frameworks")
```

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| C++17 native plugin | Python/Lua scripting | Never for this use case — scripts cannot register custom video sources with mouse interaction callbacks. Scripts are limited to properties UI manipulation and scene manipulation. |
| libobs gs_render (vector drawing) | Cairo / Skia / AGG | Only if OBS's graphics API proved too limited (e.g., anti-aliasing quality unacceptable). Avoid — introduces a heavyweight dep and a separate rendering context that must be composited back into OBS's graphics thread. |
| obs_source_info (OBS_SOURCE_TYPE_INPUT) | Filter plugin | A filter sits on another source and can't receive mouse input in the OBS preview directly. An input source IS the canvas. For a drawing overlay that users click on, input source is correct. |
| obs-frontend-api dock | Standalone Qt window | Docks integrate into OBS's layout and survive session restore. A floating window is annoying UX and doesn't persist. Use `obs_frontend_add_dock_by_id()` — not the deprecated `obs_frontend_add_dock()`. |
| QTabletEvent (Qt6 native) | RawInput / OS tablet APIs | Qt6's QTabletEvent covers Wacom and most stylus input on macOS. OBS itself does not pass pen pressure through its interaction callbacks — tablet input must come from the Qt side (the dock's draw panel or an intercepted QEvent), not from libobs's `mouse_move`. |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| Qt5 | OBS 28+ dropped Qt5; OBS 32 will refuse to load Qt5 plugins. Binary ABI is incompatible. | Qt6 (exact version matching OBS runtime) |
| Homebrew Qt6 linked into a plugin loaded by OBS.app | Homebrew currently ships Qt 6.9.x; OBS.app bundles Qt 6.8.3. Mixing Qt versions causes "Cannot mix incompatible Qt library" at runtime — plugin loads but crashes. | Link against Qt inside OBS.app's frameworks directly |
| obs-draw's pixel-painting architecture | Renders all marks into a persistent texture, modifying it each frame. Selective deletion requires re-painting the full scene, and simultaneous read/write on the GPU texture buffer is the root cause of obs-draw's crash. | Object-model: maintain a `std::vector<Mark>` render list; re-render all marks to a clean texrender each frame in `video_render`. Stateless per-frame render is crash-free. |
| gs_texrender double-buffer swap (obs-draw pattern) | obs-draw maintains two render textures (render_a, render_b) and swaps active/inactive. The race condition between the render thread reading one buffer while the main thread writes the other is where crashes originate. | Single texrender, cleared and redrawn each video_render call. With ~10 marks upper bound, full re-render per frame is trivial cost. |
| Pixel/raster canvas (painting into a texture directly) | Makes selective deletion O(full repaint), makes undo require snapshot history, and creates GPU resource contention. | Vector object model with per-frame stateless render |
| `obs_frontend_add_dock()` | Deprecated since OBS 30.0. Does not properly dock the widget. | `obs_frontend_add_dock_by_id()` |
| CMake < 3.28 | obs-plugintemplate explicitly requires 3.28+. Lower versions will fail configure with cryptic errors. | CMake 3.30.5 (macOS recommended) |

---

## Stack Patterns by Variant

**For the drawing canvas (the OBS source):**
- Register as `OBS_SOURCE_TYPE_INPUT` with `OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_CUSTOM_DRAW`
- `video_render` callback owns all drawing: clear texrender, enter graphics context, iterate mark list, call `gs_render_start` / `gs_vertex2f` / `gs_render_stop` for each mark
- `mouse_move` and `mouse_click` callbacks update the in-progress mark and the committed mark list

**For the dock panel (Qt UI):**
- Subclass `QDockWidget` or `QFrame`
- Include `<obs-frontend-api.h>` and register via `obs_frontend_add_dock_by_id()`
- Handle tablet events via `QTabletEvent` override on the preview widget
- Wire tool buttons and color swatches to the source's properties via `obs_source_update()`

**For hotkeys:**
- Register via `obs_hotkey_register_source()` in the source's `create` callback
- Serialize hotkey IDs in `get_settings` / `update` so they survive OBS restarts
- Scene transition clear: subscribe via `obs_frontend_add_event_callback()` watching `OBS_FRONTEND_EVENT_SCENE_CHANGED`

**If macOS is the only target (MVP):**
- Build universal binary (arm64 + x86_64) for distribution via `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`
- OBS.app on macOS is a universal binary — your plugin must be too

---

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| Plugin built against OBS 32.x / Qt 6.8.3 | OBS 32.x on macOS | Must match. OBS refuses to load plugins built for a newer release (hardened in OBS 32). |
| CMake 3.28–3.30 | obs-plugintemplate | Template sets `cmake_minimum_required(VERSION 3.28...3.30)`. CMake 3.31+ outside the range is accepted but triggers deprecation warnings. |
| C++17 | Qt6, libobs | Qt6 requires C++17 minimum. libobs itself is C99 but mixing with C++ is normal. |
| Xcode 16+ | macOS 14.5+ build host | Build host requirement only. Distributed plugin can target macOS 12+. |
| QTabletEvent | Qt 6.8.x | API stable across Qt6. Provides `pressure()`, `xTilt()`, `yTilt()`. Available in QInputEvent subclass since Qt 6.0. |

---

## Sources

- [OBS Plugin Template — GitHub](https://github.com/obsproject/obs-plugintemplate) — CMake version bounds (3.28...3.30), Qt6 requirement, Xcode 16 / macOS 14.5 build host (MEDIUM confidence — from README and wiki)
- [OBS Studio Plugins docs — docs.obsproject.com](https://docs.obsproject.com/plugins) — Plugin type registration, `obs_module_load`, `OBS_DECLARE_MODULE()` pattern (HIGH confidence — official docs)
- [Core Graphics API — docs.obsproject.com](https://docs.obsproject.com/reference-libobs-graphics-graphics) — `gs_draw_mode` enum, `gs_vertex2f`, `gs_render_start/stop`, `gs_texrender_*`, blend modes (HIGH confidence — official docs)
- [Source API Reference — docs.obsproject.com](https://docs.obsproject.com/reference-sources) — `obs_source_info` callbacks: `video_render`, `mouse_click`, `mouse_move`, `OBS_SOURCE_INTERACTION`, `OBS_SOURCE_CUSTOM_DRAW` (HIGH confidence — official docs)
- [OBS Frontend API — docs.obsproject.com](https://docs.obsproject.com/reference-frontend-api) — `obs_frontend_add_dock_by_id()`, `obs_frontend_add_event_callback()`, deprecated `obs_frontend_add_dock()` (HIGH confidence — official docs)
- [libobs/graphics/graphics.h — GitHub](https://github.com/obsproject/obs-studio/blob/master/libobs/graphics/graphics.h) — Authoritative function signatures and enumerations (HIGH confidence — source of truth)
- [obs-draw — GitHub (exeldro)](https://github.com/exeldro/obs-draw) — Reference implementation confirming: `gs_texrender` double-buffer pattern, `DrawDock : QFrame`, `OBSQTDisplay`, `gs_vertbuffer_t` member, `obs-frontend-api.h` include, `QTabletEvent` integration (HIGH confidence — directly read source)
- [obs-draw draw-source.c analysis](https://github.com/exeldro/obs-draw/blob/master/draw-source.c) — Confirmed pixel-painting via persistent texrender, `tablet_proc_handler` for pressure (HIGH confidence)
- [OBS Studio 32.1.0 release — GitHub](https://github.com/obsproject/obs-studio/releases) — Latest stable is 32.1.0 (March 11 2026) (HIGH confidence)
- [OBS 32.0 release: Qt 6.8 framework, dropped Qt5 plugin loading](https://obsproject.com/blog/obs-studio-32-0-release-notes) — Confirms Qt6.8 and plugin version enforcement (MEDIUM confidence — text paraphrased, exact Qt subversion via secondary source)
- [obs-stabilizer macOS build discovery](https://github.com/azumag/obs-stabilizer) — OBS.app framework path discovery pattern, Qt version mismatch diagnosis (MEDIUM confidence — community plugin, not official)

---

*Stack research for: Native OBS Studio C++ drawing overlay plugin (telestrator)*
*Researched: 2026-03-23*
