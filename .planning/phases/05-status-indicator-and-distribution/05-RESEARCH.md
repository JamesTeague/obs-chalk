# Phase 5: Status Indicator and Distribution — Research

**Researched:** 2026-04-08
**Domain:** OBS Qt dock panel, macOS codesigning/notarization, Windows x64 distribution
**Confidence:** HIGH (dock API from direct header inspection; build infrastructure from codebase; codesigning from official Apple docs + existing CI scaffolding)

## Summary

Phase 5 has two nearly independent tracks: (1) a Qt dock panel showing read-only chalk state, and (2) triggering the distribution build pipeline that the obs-plugintemplate CI already implements but has not been exercised yet. The two tracks share a single phase but have no technical dependency on each other.

**Dock panel track:** The OBS frontend API provides `obs_frontend_add_dock_by_id()` (available since OBS 30.0, present in the 31.x headers already linked). A plain `QWidget` subclass with a `QLabel`-based layout is sufficient for the read-only display. State propagation from `ChalkSource` uses Qt signals/slots through the `chalk-mode.cpp` module, which already sits on the Qt main thread and already holds the `s_chalk_mode_active` flag.

**Distribution track:** The `.github/` CI scaffolding is complete and already implements the full codesign, notarize, and package pipeline via `build-macos`, `package-macos`, `Build-Windows.ps1`, and the `setup-macos-codesigning` composite action. Distribution is gated on having Apple Developer ID credentials configured as GitHub Actions secrets and running a tag-triggered push. The Windows build runs on GitHub-hosted `windows-2022` and produces a loadable DLL with no additional tooling.

**Primary recommendation:** Build the dock panel first (one new file `chalk-dock.cpp/.hpp` + minimal additions to `chalk-mode.cpp` and `plugin.cpp`). Then validate distribution by triggering a GitHub Actions run with a version tag to exercise the full codesign/notarize/package pipeline.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Status indicator dock panel (not full toolbar):** OBS dock panel (Qt widget) that shows current state: active tool, selected color, chalk mode on/off. Read-only for v1 — displays state, not clickable for tool/color selection. Dockable: user positions it wherever they want in OBS layout. Not part of the stream output — viewers don't see it. Serves the streaming use case: stream deck sends hotkey commands, dock shows what state you're in.
- **macOS codesigned build (DIST-01):** Existing requirement, no changes from original scope.
- **Windows x64 build (DIST-02):** Existing requirement, no changes from original scope. MVP must be usable on the streaming machine (Windows).

### Claude's Discretion
- Dock panel layout and visual design
- How state updates propagate from ChalkSource to the dock widget (signal/slot, polling, etc.)
- Whether dock shows delete mode and chalk mode as separate indicators or combined

### Deferred Ideas (OUT OF SCOPE)
- Full clickable toolbar with tool/color buttons: v1.1
- Stream deck integration via OBS WebSocket protocol: future
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| UI-01 | Qt dock panel in OBS for tool selection and color palette display | `obs_frontend_add_dock_by_id()` is available in the 31.x headers; panel is read-only per locked decisions |
| DIST-01 | macOS universal binary (Intel + Apple Silicon), codesigned and notarized | CI scaffolding complete in `.github/`; requires Developer ID secrets configured; notarization triggered by semver tag push |
| DIST-02 | Windows x64 build produces a loadable plugin DLL | `windows-x64` cmake preset exists; `Build-Windows.ps1` is present; runs on `windows-2022` runner |
</phase_requirements>

## Standard Stack

### Core (no new external dependencies)
| API | Version | Purpose | Notes |
|-----|---------|---------|-------|
| `obs_frontend_add_dock_by_id` | OBS 31.x (added 30.0) | Register dock panel widget with OBS UI | Takes `const char *id`, `const char *title`, `void *widget` (QWidget*); returns bool |
| `obs_frontend_remove_dock` | OBS 31.x | Remove dock on plugin unload | Takes `const char *id` |
| Qt6 Widgets | 6.x (already linked) | `QWidget`, `QLabel`, `QVBoxLayout`, `QHBoxLayout` | Already a dependency; no new packages |
| Qt signals/slots | Qt6 | State propagation from chalk-mode to dock | Dock widget can connect to a signal emitted when chalk state changes |

### Build / Distribution
| Tool | Version | Purpose | Notes |
|------|---------|---------|-------|
| GitHub Actions | CI | macOS + Windows builds | Workflows already in `.github/workflows/` |
| Xcode 16.1 | macOS-15 runner | macOS universal binary via xcodebuild | CI workflow pins `Xcode_16.1.0.app` |
| Visual Studio 2022 | windows-2022 runner | Windows x64 DLL | `windows-ci-x64` CMake preset |
| `pkgbuild` + `productbuild` | macOS system | Creates `.pkg` installer | Already in `create-package.cmake.in` |
| `xcrun notarytool` | macOS Xcode CLI | Notarization submission + staple | `altool` is deprecated; `notarytool` is required |
| `productsign` | macOS system | Signs the `.pkg` installer | Done in `package-macos` script |

**Installation:** No new packages. All APIs available through existing linkage.

## Architecture Patterns

### Recommended Project Structure (additions only)
```
src/
├── chalk-dock.cpp/.hpp   # NEW: ChalkDock QWidget subclass + registration
├── chalk-mode.cpp/.hpp   # MODIFIED: emit state-change signal, call dock install/shutdown
├── plugin.cpp            # MODIFIED: call chalk_dock_install/shutdown from on_obs_event
└── (existing files unchanged)
```

### Pattern 1: obs_frontend_add_dock_by_id

**What:** Register a plain `QWidget` as a dockable panel. OBS wraps it in a `QDockWidget` internally, adds a toggle entry to the Docks menu, and persists dock position across restarts.

**API (from `.deps/include/obs/obs-frontend-api.h`):**
```cpp
// Takes QWidget* cast to void*, returns true on success
EXPORT bool obs_frontend_add_dock_by_id(const char *id, const char *title, void *widget);

// Remove dock on unload
EXPORT void obs_frontend_remove_dock(const char *id);
```

**When to call:** In `chalk_mode_install()` (called from `OBS_FRONTEND_EVENT_FINISHED_LOADING`). This is the correct point because OBS UI is fully initialized at that event. Same lifecycle used for the existing event filter.

**Widget ownership:** OBS takes ownership of the widget after `obs_frontend_add_dock_by_id`. Do not delete it manually. `obs_frontend_remove_dock` handles cleanup.

**Example — minimal dock registration:**
```cpp
// In chalk_mode_install():
auto *dock_widget = new ChalkDock();
obs_frontend_add_dock_by_id("chalk-status", obs_module_text("ChalkStatus"), dock_widget);
s_dock = dock_widget;  // store pointer for state updates
```

### Pattern 2: State Propagation via Direct Call

**What:** Since `chalk-mode.cpp` already calls `chalk_find_source()` to dispatch events and holds `s_chalk_mode_active`, the simplest propagation is a direct function call to the dock widget when state changes. No polling timer required.

**When to update:** State changes in three places:
1. `chalk_mode_hotkey_cb` — chalk mode toggled on/off (in chalk-mode.cpp)
2. Hotkey callbacks in chalk-source.cpp — tool selection or color change

For dock updates triggered from source hotkeys (chalk-source.cpp), the update pathway needs to reach the dock. Two clean options:

**Option A — Direct call from chalk_mode.cpp side on mode change + periodic sync on hotkey:**
`chalk-mode.cpp` owns the dock pointer (`s_dock`). When `chalk_mode_hotkey_cb` fires, call `s_dock->update_state(chalk_mode_active, ctx->tool_state)`. For tool/color changes from source hotkeys, emit an update after the hotkey changes state by querying `chalk_find_source()` from the hotkey callback location — but source hotkeys live in `chalk-source.cpp` which would need to know about the dock.

**Option B — Qt signal from a shared notifier (recommended):**
Add a simple `ChalkStateNotifier` singleton (or a static Qt object) that any code can call: `ChalkStateNotifier::instance().notify()`. The dock connects to this signal. Avoids cross-file coupling.

**Option C — OBS frontend save callback for persistence + polling timer:**
Dock polls `chalk_find_source()` on a 100ms `QTimer`. Simple, no coupling, acceptable latency for a status display.

**Recommended: Option A (direct call) for mode toggle, Option C (100ms timer) for tool/color:**
- Mode toggle: `chalk-mode.cpp` already has the active flag and calls `chalk_update_cursor()`; adding a dock update there is zero-coupling.
- Tool/color: The hotkeys fire infrequently; 100ms polling via `QTimer` in the dock is invisible to users and requires no changes to `chalk-source.cpp`.

### Pattern 3: ChalkDock Widget Layout

**What:** A compact `QWidget` with `QLabel` rows for each state item.

**Design (Claude's discretion — read-only v1):**
```
┌─────────────────────────────┐
│ Chalk Mode: [ACTIVE / off]  │
│ Tool:       Freehand        │
│ Color:      ●  White        │
└─────────────────────────────┘
```

**Implementation sketch:**
```cpp
// chalk-dock.hpp
#pragma once
#include <QWidget>
#include <QLabel>
#include "tool-state.hpp"

class ChalkDock : public QWidget {
    Q_OBJECT
public:
    explicit ChalkDock(QWidget *parent = nullptr);
    void set_chalk_mode(bool active);
    void set_tool_state(ToolType tool, int color_index);
private:
    QLabel *mode_label_;
    QLabel *tool_label_;
    QLabel *color_label_;
};
```

**Tool name mapping (pure string table, no new dependencies):**
```cpp
static const char *tool_name(ToolType t) {
    switch (t) {
        case ToolType::Freehand: return "Freehand";
        case ToolType::Arrow:    return "Arrow";
        case ToolType::Circle:   return "Circle";
        case ToolType::Cone:     return "Cone";
        case ToolType::Laser:    return "Laser";
        case ToolType::Delete:   return "Delete";
    }
    return "Unknown";
}
```

**Color swatch:** Set `QLabel` background-color via `setStyleSheet` using the palette ABGR value converted to CSS hex. Palette values are ABGR; CSS needs #RRGGBB: swap R and B bytes.

### Pattern 4: macOS Codesigning + Notarization Pipeline

**What:** The `.github/` CI already implements the complete pipeline. The gate is: required secrets must be configured in the GitHub repository.

**CI flow (from `build-project.yaml`):**
1. On push of a semver tag (e.g., `0.1.0`): `notarize:true`, `config:Release`
2. `setup-macos-codesigning` action: imports PKCS12 cert from `MACOS_SIGNING_CERT` secret into ephemeral keychain
3. `build-plugin` action: calls `.github/scripts/build-macos --codesign`; Xcode signs `.plugin` bundle using `CODESIGN_IDENT`
4. `package-plugin` action: calls `.github/scripts/package-macos --codesign --notarize --package`; `productsign` signs `.pkg`; `xcrun notarytool submit ... --wait`; `xcrun stapler staple`

**Required GitHub secrets:**
| Secret | Content |
|--------|---------|
| `MACOS_SIGNING_APPLICATION_IDENTITY` | Developer ID Application: Name (TEAMID) |
| `MACOS_SIGNING_INSTALLER_IDENTITY` | Developer ID Installer: Name (TEAMID) |
| `MACOS_SIGNING_CERT` | base64-encoded PKCS12 (.p12) certificate |
| `MACOS_SIGNING_CERT_PASSWORD` | password for the .p12 |
| `MACOS_KEYCHAIN_PASSWORD` | password for ephemeral keychain (any random string) |
| `MACOS_NOTARIZATION_USERNAME` | Apple ID email |
| `MACOS_NOTARIZATION_PASSWORD` | app-specific password from appleid.apple.com |
| `MACOS_SIGNING_PROVISIONING_PROFILE` | base64-encoded .provisionprofile (optional for plug-ins) |

**Manual prerequisite (human step, not automated):** Developer ID cert must be obtained from developer.apple.com — requires paid Apple Developer Program membership. This is a manual/collaborative step, not autonomous.

**Entitlements:** No entitlements file exists yet at `cmake/macos/entitlements.plist`. OBS plugins are typically signed without entitlements or with a minimal hardened-runtime entitlement. The CMake helper checks for this file and uses it if present — no action needed if it doesn't exist.

### Pattern 5: Windows x64 Distribution

**What:** The `windows-x64` and `windows-ci-x64` CMake presets plus `Build-Windows.ps1` and `Package-Windows.ps1` handle the full build and packaging.

**Build output structure (from `cmake/windows/helpers.cmake`):**
```
release/RelWithDebInfo/
└── obs-chalk/
    └── bin/
        └── 64bit/
            └── obs-chalk.dll
            └── obs-chalk.pdb  (debug symbols)
```

**User install path (from OBS plugins guide):**
```
C:\ProgramData\obs-studio\plugins\obs-chalk\bin\64bit\obs-chalk.dll
```

**CI build:** `windows-build` job runs on `windows-2022` with Visual Studio 2022. The `windows-ci-x64` preset enables `CMAKE_COMPILE_WARNING_AS_ERROR`. Dependencies (obs-studio + prebuilt + qt6) are downloaded from `buildspec.json` by the cmake bootstrap step. No manual toolchain setup needed.

**Windows distribution artifact:** CI uploads `obs-chalk-{version}-windows-x64-{hash}` — a zip containing the install-ready directory tree. Users unzip into their OBS plugins folder.

### Anti-Patterns to Avoid

- **Using deprecated `obs_frontend_add_dock()`:** Takes a `QDockWidget*` directly and is marked `OBS_DEPRECATED`. Use `obs_frontend_add_dock_by_id()` which takes a plain `QWidget*` and OBS wraps it.
- **Manually parenting the dock widget to QMainWindow:** `obs_frontend_add_dock_by_id` handles this. Do not call `setParent` on the widget before registering.
- **Calling dock registration before OBS_FRONTEND_EVENT_FINISHED_LOADING:** OBS UI is not fully initialized before this event. The existing `chalk_mode_install` pattern is the right lifecycle hook.
- **Using `xcrun altool` for notarization:** Deprecated since Fall 2023; use `xcrun notarytool submit`. The existing `package-macos` script already uses `notarytool`.
- **Deleting the dock widget manually:** OBS takes ownership after `obs_frontend_add_dock_by_id`. Call `obs_frontend_remove_dock` on shutdown; do not `delete s_dock`.
- **Hardcoding ABGR vs ARGB in color label:** OBS palette is ABGR. To convert to CSS #RRGGBB: `r = abgr & 0xFF`, `g = (abgr >> 8) & 0xFF`, `b = (abgr >> 16) & 0xFF`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Dock registration and persistence | Custom QDockWidget management | `obs_frontend_add_dock_by_id` | OBS persists dock geometry in its layout file; custom management would fight this |
| macOS code signing CI | Custom codesign scripts | Existing `.github/scripts/build-macos` + `package-macos` + `setup-macos-codesigning` | Already fully implemented for this repo |
| Windows build pipeline | Custom MSBuild scripts | Existing `Build-Windows.ps1` + `Package-Windows.ps1` + `windows-ci-x64` preset | Already fully implemented |
| Color display | Custom color widget | `QLabel` with `setStyleSheet("background-color: #RRGGBB")` | Sufficient for read-only status display |

## Common Pitfalls

### Pitfall 1: Dock registration timing
**What goes wrong:** `obs_frontend_add_dock_by_id` is called before OBS UI is fully initialized, crashing or silently failing.
**Why it happens:** Plugin code that runs in `obs_module_load` runs before the main window is fully constructed.
**How to avoid:** Call dock registration inside `chalk_mode_install()`, which is already gated on `OBS_FRONTEND_EVENT_FINISHED_LOADING`. This is the established pattern in this plugin.
**Warning signs:** OBS logs show the dock ID but it doesn't appear in the Docks menu.

### Pitfall 2: State update from wrong thread
**What goes wrong:** A hotkey callback fires on a background thread and calls `QLabel::setText()` directly, causing Qt cross-thread GUI warnings or silent corruption.
**Why it happens:** OBS hotkey callbacks run on the OBS input thread, not the Qt main thread.
**How to avoid:** Use `QMetaObject::invokeMethod(dock, [=]{ dock->set_tool_state(...); }, Qt::QueuedConnection)` for updates from hotkey callbacks. The 100ms `QTimer` approach avoids this entirely because the timer fires on the Qt main thread.
**Warning signs:** `QObject: Cannot create children for a parent that is in a different thread` in OBS log.

### Pitfall 3: Dock ID collision
**What goes wrong:** Another plugin uses the same dock ID string, causing silent failure or unexpected behavior.
**How to avoid:** Use a namespace prefix: `"chalk-status"` or `"obs-chalk-status"`. This is unlikely to collide.

### Pitfall 4: macOS notarization requires hardened runtime
**What goes wrong:** Notarization is rejected with "The binary is not hardened" error.
**Why it happens:** Apple requires `--options=runtime` (hardened runtime) for all notarized binaries.
**How to avoid:** The Xcode generator sets `XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME` in the obs-plugintemplate defaults. Verify this is set in `cmake/macos/defaults.cmake`. If missing, add: `XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES` to the target properties.
**Warning signs:** `xcrun notarytool submit` returns `Invalid` status.

### Pitfall 5: Windows DLL missing Qt runtime
**What goes wrong:** Plugin loads but immediately crashes because Qt DLLs are not found on the streaming machine.
**Why it happens:** Qt is a runtime dependency; OBS ships Qt DLLs alongside its binary. The plugin DLL links against the same Qt version OBS uses, so they must match.
**How to avoid:** The `windows-ci-x64` preset links against the prebuilt Qt from `buildspec.json` (version `2025-07-11`), which matches OBS 31.x's Qt. OBS 31.x installs Qt DLLs into its own directory. Since the plugin DLL goes into the OBS plugins folder, OBS's own Qt DLLs on the streaming machine provide the runtime. No separate Qt install needed.
**Warning signs:** DLL loads but crashes immediately on OBS launch; check Windows Event Viewer for missing DLL errors.

### Pitfall 6: QTimer polling holding a reference to deleted source
**What goes wrong:** `QTimer` fires after OBS scene changes and `chalk_find_source()` returns nullptr; dock tries to dereference.
**How to avoid:** Always null-check the result of `chalk_find_source()` before accessing fields. If nullptr, display a "No source" or grayed state in the dock. This null-check pattern is already used everywhere `chalk_find_source()` is called in this codebase.

## Code Examples

### Dock registration in chalk_mode_install
```cpp
// chalk-mode.cpp (chalk_mode_install additions)
// After finding s_preview and registering the hotkey:

#include "chalk-dock.hpp"

static ChalkDock *s_dock = nullptr;

// ... inside chalk_mode_install():
s_dock = new ChalkDock();
obs_frontend_add_dock_by_id("chalk-status",
                             obs_module_text("ChalkStatus"),
                             s_dock);
blog(LOG_INFO, "obs-chalk: chalk dock installed");
```

### Dock removal in chalk_mode_shutdown
```cpp
// chalk-mode.cpp (chalk_mode_shutdown additions)
if (s_dock) {
    obs_frontend_remove_dock("chalk-status");
    // OBS owns the widget after add_dock_by_id; do NOT delete s_dock.
    s_dock = nullptr;
}
```

### ChalkDock with QTimer polling
```cpp
// chalk-dock.cpp
#include "chalk-dock.hpp"
#include "chalk-source.hpp"
#include "tool-state.hpp"
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>

ChalkDock::ChalkDock(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    mode_label_  = new QLabel("Chalk Mode: off", this);
    tool_label_  = new QLabel("Tool: Freehand", this);
    color_label_ = new QLabel("Color: White", this);
    layout->addWidget(mode_label_);
    layout->addWidget(tool_label_);
    layout->addWidget(color_label_);
    layout->addStretch();
    setLayout(layout);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ChalkDock::refresh);
    timer->start(100); // 100ms polling — invisible latency for status display
}

void ChalkDock::refresh()
{
    // Update chalk mode label from extern state in chalk-mode.cpp
    bool mode = chalk_mode_is_active(); // new accessor in chalk-mode.cpp
    mode_label_->setText(mode ? "Chalk Mode: ACTIVE" : "Chalk Mode: off");

    ChalkSource *ctx = chalk_find_source();
    if (!ctx) {
        tool_label_->setText("Tool: (no source)");
        color_label_->setText("Color: —");
        return;
    }

    tool_label_->setText(
        QString("Tool: %1").arg(tool_name(ctx->tool_state.active_tool)));

    uint32_t abgr = ctx->tool_state.active_color();
    uint8_t r = abgr & 0xFF;
    uint8_t g = (abgr >> 8) & 0xFF;
    uint8_t b = (abgr >> 16) & 0xFF;
    color_label_->setStyleSheet(
        QString("background-color: #%1%2%3; color: %4;")
            .arg(r, 2, 16, QChar('0'))
            .arg(g, 2, 16, QChar('0'))
            .arg(b, 2, 16, QChar('0'))
            .arg((r + g + b > 382) ? "black" : "white"));
    color_label_->setText(color_name(ctx->tool_state.color_index));
}
```

### chalk_mode_is_active accessor (new in chalk-mode.cpp)
```cpp
// chalk-mode.hpp addition:
bool chalk_mode_is_active();

// chalk-mode.cpp addition:
bool chalk_mode_is_active() { return s_chalk_mode_active; }
```

### macOS hardened runtime verification
```bash
# Verify after build:
codesign -dv --verbose=4 build_macos/RelWithDebInfo/obs-chalk.plugin 2>&1 | grep -i hardened
# Should show: CodeDirectory flags: 0x10000(runtime)
```

### Triggering CI codesign + notarize
```bash
# Tag a release to trigger notarize:true in build-project.yaml:
git tag 0.1.0
git push origin 0.1.0
# CI sees GITHUB_REF_NAME matching semver pattern → notarize:true, config:Release
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `obs_frontend_add_dock()` (takes QDockWidget*) | `obs_frontend_add_dock_by_id()` (takes plain QWidget*) | OBS 30.0 | Simpler; no manual QDockWidget wrapping |
| `xcrun altool --notarize-app` | `xcrun notarytool submit` | Fall 2023 | altool rejected by Apple; existing package-macos script already uses notarytool |
| Manual plugin install (copy DLL) | NSIS/zip installer from cmake install | obs-plugintemplate standard | cmake install target produces correct directory structure |

**Deprecated/outdated:**
- `obs_frontend_add_dock`: deprecated in OBS 30.0; use `obs_frontend_add_dock_by_id`
- `xcrun altool`: removed by Apple, notarytool required

## Open Questions

1. **Hardened runtime entitlement — is it set in defaults.cmake?**
   - What we know: `cmake/macos/defaults.cmake` was not read during this research session
   - What's unclear: Whether `XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES` is already set
   - Recommendation: Read `cmake/macos/defaults.cmake` at plan time; if missing, add it as a task. Notarization will fail without it.

2. **Apple Developer Program membership**
   - What we know: Requires paid membership ($99/year) to obtain a Developer ID cert
   - What's unclear: Whether Teague has an active Apple Developer account
   - Recommendation: This is a manual/collaborative step. The CI pipeline is ready; the human must provide secrets. Flag as a prerequisite before DIST-01 can be verified.

3. **Windows testing on streaming machine**
   - What we know: DIST-02 requires the DLL to run on the Windows streaming machine
   - What's unclear: Whether the streaming machine has OBS installed and which version
   - Recommendation: Build CI artifact first; manual install test on streaming machine is the verification gate. Flag as human-in-the-loop step.

4. **Dock persistence across OBS restarts**
   - What we know: `obs_frontend_add_dock_by_id` causes OBS to persist the dock geometry in its layout file
   - What's unclear: Whether dock visibility (open/closed) is also persisted or only position
   - Recommendation: Test manually after first installation. Success criterion says "persists across restarts" — verify this during the verification phase.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None — C++ OBS plugin; no automated test infrastructure |
| Config file | none |
| Quick run command | `cmake --build build_macos --config RelWithDebInfo 2>&1 \| tail -5` |
| Full suite command | Build clean + load in OBS + visual verification |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| UI-01 | Dock panel appears in OBS UI showing active tool, color, chalk mode | manual | build clean | ❌ |
| UI-01 | Dock persists across OBS restarts | manual | n/a | ❌ |
| UI-01 | Dock updates in real-time when state changes via hotkey | manual | n/a | ❌ |
| DIST-01 | macOS .pkg installs without Gatekeeper warning | manual (CI triggers build) | `git tag 0.1.0 && git push origin 0.1.0` | ❌ |
| DIST-02 | Windows DLL loads in OBS on streaming machine | manual | CI windows-build artifact | ❌ |

All tests are manual-only. This is a visual/distribution plugin; OBS running with live scene is required for behavioral testing, and distribution testing requires actual hardware.

### Sampling Rate
- **Per task commit:** `cmake --build build_macos --config RelWithDebInfo 2>&1 | tail -5`
- **Per wave merge:** Build clean + load in OBS + visual dock verification
- **Phase gate:** All behaviors verified in OBS; DIST-01/DIST-02 require human steps with CI

### Wave 0 Gaps
None — no test infrastructure changes needed. Build verification is the gate.

## Sources

### Primary (HIGH confidence)
- `/Users/jteague/dev/obs-chalk/.deps/include/obs/obs-frontend-api.h` — `obs_frontend_add_dock_by_id`, `obs_frontend_remove_dock` signatures confirmed directly from the OBS 31.1.1 headers linked by this project
- `/Users/jteague/dev/obs-chalk/.github/` — full CI pipeline confirmed from direct codebase inspection: `build-project.yaml`, `setup-macos-codesigning/action.yaml`, `build-macos`, `package-macos`, `Build-Windows.ps1`, `Package-Windows.ps1`
- `/Users/jteague/dev/obs-chalk/cmake/macos/helpers.cmake` — macOS bundle structure, pkgbuild/productbuild calls
- `/Users/jteague/dev/obs-chalk/cmake/windows/helpers.cmake` — Windows install target structure
- `/Users/jteague/dev/obs-chalk/buildspec.json` — confirmed OBS 31.1.1, prebuilt 2025-07-11, qt6 2025-07-11

### Secondary (MEDIUM confidence)
- [OBS Studio Frontend API docs](https://docs.obsproject.com/reference-frontend-api) — confirmed `obs_frontend_add_dock_by_id` added in 30.0
- [OBS Plugins Guide](https://obsproject.com/kb/plugins-guide) — confirmed Windows install path: `C:\ProgramData\obs-studio\plugins\{name}\bin\64bit\`
- [Apple Developer ID](https://developer.apple.com/developer-id/) — confirmed Developer ID Application cert requirement for distribution

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack (dock API): HIGH — confirmed from direct header inspection of OBS 31.x headers
- Standard stack (distribution): HIGH — full CI pipeline present and inspected
- Architecture patterns: HIGH — all patterns derived from existing working code
- Pitfalls: HIGH — timing issues and thread safety from established codebase patterns; Apple notarization requirements from official docs

**Research date:** 2026-04-08
**Valid until:** 60 days for dock API (stable OBS API); 30 days for Apple notarization requirements (Apple policy changes occasionally)
