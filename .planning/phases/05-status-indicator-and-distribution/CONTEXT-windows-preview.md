# CONTEXT: Windows Preview Interaction Fix

## Problem

Qt event filter (`QObject::eventFilter`) on the OBS preview widget works on macOS but fails on Windows. Root cause: `OBSQTDisplay` sets `WA_NativeWindow` in its constructor, which creates a separate HWND. On Windows, mouse events go through the Windows message pump and are dispatched directly to `OBSBasicPreview::mousePressEvent` — they never enter Qt's `eventFilter()` chain.

Symptom: chalk mode hotkey works, crosshair cursor appears, but clicking the preview creates OBS's selection box instead of drawing.

## Decision: QCoreApplication::installNativeEventFilter()

Evaluated three options:

1. **QCoreApplication::installNativeEventFilter()** — CHOSEN
2. **nativeEvent() override on subclass** — rejected (can't override virtuals on objects we don't own from a plugin)
3. **SetWindowSubclass() on preview HWND** — rejected (lifecycle complexity, Win32/Qt mixing, coordinate translation burden)

## Research Findings (OBS codebase)

Confirmed via obsproject/obs-studio source review:

- **OBS has one native event filter** (`OBS::NativeEventFilter` in `frontend/utility/`). Handles only `WM_QUERYENDSESSION` / `WM_ENDSESSION`. Returns `false` for everything else. No conflict.
- **OBS does zero custom message handling on the preview HWND.** No `SetWindowSubclass`, no `WndProc`, no `WM_LBUTTONDOWN` anywhere in frontend code.
- **OBSBasicPreview mouse state is consumption-safe.** If `mousePressEvent` never fires, `mouseDown` stays `false`, and move/release handlers are no-ops. No partial state corruption.
- **Preview HWND is stable for the entire app lifecycle.** Created once from .ui file, never rebuilt during scene changes, studio mode toggles, or display settings changes. Safe to cache via `(HWND)preview->winId()`.
- **OBSQTDisplay::nativeEvent** only handles `WM_DISPLAYCHANGE` for color space. Returns `false` for all else.

## Implementation

### Architecture
- `#ifdef _WIN32`: create a `QAbstractNativeEventFilter` subclass (`ChalkNativeFilter`)
- In `nativeEventFilter()`: check `msg->hwnd == cachedPreviewHwnd` AND `s_chalk_mode_active`
  - Match: translate WM_LBUTTONDOWN/WM_MOUSEMOVE/WM_LBUTTONUP to chalk_input_begin/move/end, return `true`
  - No match: return `false` (passthrough)
- macOS path: existing `eventFilter()` unchanged
- Coordinate mapping: reuse `preview_widget_to_scene()` — convert Win32 client coords (LOWORD/HIWORD of lParam) to logical pixels, then through existing mapping
- Install in `chalk_mode_install()`, remove in `chalk_mode_shutdown()`

### Scope
- Mouse events only (WM_LBUTTONDOWN, WM_MOUSEMOVE, WM_LBUTTONUP)
- Tablet/pen events on Windows: deferred. WM_POINTER or Wintab would be needed. Mouse is sufficient for MVP — Teague's streaming machine uses mouse, not tablet.
- Right-click passthrough — only intercept left button

### Cleanup
- Two debug commits on HEAD (`7881f74` event filter hit logging, `c79f6be` widget name dump) must be cleaned up. Strip the debug-only logging (blog calls that exist purely for diagnosis). The widget class/children log in `chalk_mode_install` is useful context — keep that. The "eventFilter got MouseButtonPress" log and the child widget name dump loop are diagnostic scaffolding — remove.

## Discretion

- Whether to put `ChalkNativeFilter` in chalk-mode.cpp or a separate file (file size constraint: ~500 lines)
- Exact Win32 message filtering (whether to also handle WM_RBUTTONDOWN for context menu passthrough or just ignore non-left-button)
- Debug logging level and content for the new path

## Deferred

- Windows tablet/pen input (WM_POINTER with IS_POINTER_FLAG_INCONTACT) — separate phase if needed
- Windows codesigning
