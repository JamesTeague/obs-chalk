---
phase: 4
slug: polish
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-04-08
---

# Phase 4 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None — C++ OBS plugin, manual visual verification |
| **Config file** | none |
| **Quick run command** | `cmake --build build/macos --config RelWithDebInfo 2>&1 \| tail -5` |
| **Full suite command** | Build clean + load in OBS + visual verification |
| **Estimated runtime** | ~30 seconds (build) |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build/macos --config RelWithDebInfo 2>&1 | tail -5`
- **After every plan wave:** Build clean + load in OBS + visual verification
- **Before `/gsd:verify-work`:** All five behaviors verified in OBS
- **Max feedback latency:** 30 seconds (build time)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| TBD | TBD | TBD | UX-01 | manual | build clean | N/A | ⬜ pending |
| TBD | TBD | TBD | UX-02 | manual | build clean | N/A | ⬜ pending |
| TBD | TBD | TBD | UX-03 | manual | build clean | N/A | ⬜ pending |
| TBD | TBD | TBD | UX-04 | manual | build clean | N/A | ⬜ pending |
| TBD | TBD | TBD | INTG-01 | manual | build clean | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. No test framework needed — this is a visual rendering plugin requiring OBS runtime verification.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Arrow/circle/cone edges render at 6px | UX-01 | Visual rendering in OBS preview | Draw each mark type, compare line weight to freehand stroke |
| Cone fill visibly transparent | UX-02 | Visual rendering in OBS preview | Draw cone over video source, verify canvas visible through fill |
| Delete mode persists | UX-03 | Interactive behavior in OBS | Toggle delete mode, click 3+ marks, verify each deletes without mode exiting |
| Laser shows on mouse-down | UX-04 | Interactive behavior in OBS | Select laser tool, verify dot appears on click, disappears on release |
| Scene-transition clear | INTG-01 | Requires OBS scene switching | Enable setting, draw marks, switch scenes, verify marks cleared |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
