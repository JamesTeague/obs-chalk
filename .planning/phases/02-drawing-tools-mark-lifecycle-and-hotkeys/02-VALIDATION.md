---
phase: 2
slug: drawing-tools-mark-lifecycle-and-hotkeys
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-30
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None — OBS plugin, all behaviors require GPU context |
| **Config file** | None |
| **Quick run command** | `cmake --build build_macos --config Debug 2>&1 \| tail -5` |
| **Full suite command** | Manual OBS verification (all tools, hotkeys, undo/delete/clear) |
| **Estimated runtime** | ~10 seconds (build), ~3 minutes (full manual checklist) |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build_macos --config Debug`
- **After every plan wave:** Full build + manual OBS verification of new functionality
- **Before `/gsd:verify-work`:** All 11 requirements verified manually in OBS
- **Max feedback latency:** 10 seconds (build)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| TBD | TBD | TBD | TOOL-01 | manual | build | N/A | ⬜ pending |
| TBD | TBD | TBD | TOOL-02 | manual | build | N/A | ⬜ pending |
| TBD | TBD | TBD | TOOL-03 | manual | build | N/A | ⬜ pending |
| TBD | TBD | TBD | TOOL-04 | manual | build | N/A | ⬜ pending |
| TBD | TBD | TBD | TOOL-05 | manual | build | N/A | ⬜ pending |
| TBD | TBD | TBD | MARK-01 | manual | build | N/A | ⬜ pending |
| TBD | TBD | TBD | MARK-02 | manual | build | N/A | ⬜ pending |
| TBD | TBD | TBD | MARK-03 | manual | build | N/A | ⬜ pending |
| TBD | TBD | TBD | MARK-04 | manual | build | N/A | ⬜ pending |
| TBD | TBD | TBD | INPT-01 | manual | build | N/A | ⬜ pending |
| TBD | TBD | TBD | INPT-02 | manual | build | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

*No test framework required — build success is the automated gate, all behavioral verification is manual in OBS.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Freehand: no gaps at fast movement | TOOL-01 | Requires OBS + mouse interaction | Draw rapidly in Interact window, verify continuous strokes |
| Arrow: shaft + arrowhead visible | TOOL-02 | Requires OBS rendering | Click-drag in Interact window, verify arrow appears |
| Circle: outline visible | TOOL-03 | Requires OBS rendering | Click-drag in Interact window, verify circle appears |
| Cone: filled semi-transparent triangle | TOOL-04 | Requires OBS rendering | Click-drag in Interact window, verify cone appears |
| Laser: appears on hold, gone on release | TOOL-05 | Requires OBS + hotkey | Hold laser hotkey, verify dot follows cursor, release to hide |
| Undo removes last mark | MARK-01 | Requires OBS + hotkey | Draw marks, press undo hotkey, verify last removed |
| Pick-delete removes specific mark | MARK-02 | Requires OBS + click interaction | Enter pick mode, click near a mark, verify only that mark removed |
| Clear all removes everything | MARK-03 | Requires OBS + hotkey | Draw marks, press clear hotkey, verify clean frame |
| Multi-undo steps back through history | MARK-04 | Requires OBS + hotkey | Draw 3+ marks, press undo repeatedly, verify each removed in order |
| All hotkeys appear in OBS settings | INPT-01 | Requires OBS UI | Open OBS Settings > Hotkeys, verify all actions listed |
| Color cycle wraps through 5 colors | INPT-02 | Requires OBS + hotkey | Press color cycle hotkey repeatedly, verify white→yellow→red→blue→green→white |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
