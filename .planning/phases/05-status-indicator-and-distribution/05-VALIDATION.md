---
phase: 5
slug: status-indicator-and-distribution
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-04-09
---

# Phase 5 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None — C++ OBS plugin, manual visual verification |
| **Config file** | none |
| **Quick run command** | `cmake --build build_macos --config RelWithDebInfo 2>&1 \| tail -5` |
| **Full suite command** | Build clean + load in OBS + visual verification |
| **Estimated runtime** | ~30 seconds (build) |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build_macos --config RelWithDebInfo 2>&1 | tail -5`
- **After every plan wave:** Build clean + load in OBS + visual verification
- **Before `/gsd:verify-work`:** All behaviors verified in OBS
- **Max feedback latency:** 30 seconds (build time)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| TBD | TBD | TBD | UI-01 | manual | build clean | N/A | ⬜ pending |
| TBD | TBD | TBD | DIST-01 | manual | build clean + codesign verify | N/A | ⬜ pending |
| TBD | TBD | TBD | DIST-02 | manual | build clean + load on Windows | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. No test framework needed.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Dock panel shows tool/color/mode state | UI-01 | Visual rendering in OBS | Open dock, switch tools/colors via hotkey, verify updates in real-time |
| Dock persists across OBS restarts | UI-01 | Requires OBS restart | Open dock, close OBS, reopen, verify dock reappears |
| macOS codesigned + notarized | DIST-01 | Requires Apple Developer cert | Build, sign, notarize, install on clean machine, verify no Gatekeeper warning |
| Windows x64 plugin loads | DIST-02 | Requires Windows machine | Build on Windows CI or cross-compile, install DLL in OBS on streaming machine |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
