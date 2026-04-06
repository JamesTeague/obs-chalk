---
phase: 03
slug: preview-interaction
status: approved
nyquist_compliant: true
wave_0_complete: true
created: 2026-04-06
---

# Phase 03 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None — C++ OBS plugin, no test runner |
| **Config file** | None — no test infrastructure expected for plugin type |
| **Quick run command** | `cmake --build .build/macos -- -j4 2>&1 \| grep -E "error:\|warning:"` |
| **Full suite command** | `cmake --build .build/macos && cp .build/macos/obs-chalk.plugin/Contents/MacOS/obs-chalk /Applications/OBS.app/Contents/PlugIns/obs-chalk.plugin/Contents/MacOS/obs-chalk` |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run quick build command — zero errors, no new warnings
- **After every plan wave:** Full build + install + OBS load, no crash on startup
- **Before `/gsd:verify-work`:** All 5 requirements manually verified in OBS
- **Max feedback latency:** 15 seconds (build time)

---

## Per-Task Verification Map

| Req ID | Behavior | Test Type | Automated Command | Status |
|--------|----------|-----------|-------------------|--------|
| PREV-01 | Event filter installed; chalk mode intercepts mouse press on preview | manual-only | Build + OBS load + click preview | ⬜ pending |
| PREV-02 | Marks render at correct position relative to canvas | manual-only | Draw at corners; verify alignment | ⬜ pending |
| PREV-03 | Hotkey toggles chalk mode; pass-through works when off | manual-only | Bind hotkey; toggle; verify source selection | ⬜ pending |
| PREV-04 | Cursor changes to crosshair when chalk mode on | manual-only | Visual inspection | ⬜ pending |
| INPT-03 | Tablet pressure varies stroke width | manual-only | Draw with tablet; vary pressure; inspect width | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. No test framework is expected for this plugin type — build-clean validation is the automated gate, manual OBS verification is the phase gate.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Event filter intercepts preview clicks | PREV-01 | Requires OBS runtime with preview widget | 1. Load OBS with obs-chalk. 2. Toggle chalk mode on. 3. Click-drag on preview. 4. Verify marks appear. |
| Coordinate mapping accuracy | PREV-02 | Requires visual verification against canvas bounds | 1. Draw marks at preview corners. 2. Verify marks align with scene edges. 3. Resize preview window. 4. Draw again; verify alignment holds. |
| Chalk mode toggle and pass-through | PREV-03 | Requires OBS hotkey system and source interaction | 1. Bind chalk mode hotkey. 2. Toggle on. 3. Verify drawing works. 4. Toggle off. 5. Verify source selection/transform handles work. |
| Cursor change on chalk mode | PREV-04 | Visual UI verification | 1. Toggle chalk mode on. 2. Hover over preview. 3. Verify crosshair cursor. 4. Toggle off. 5. Verify default cursor restored. |
| Tablet pressure sensitivity | INPT-03 | Requires physical tablet hardware | 1. Connect tablet. 2. Toggle chalk mode on. 3. Draw with light pressure. 4. Draw with heavy pressure. 5. Verify stroke width difference. |

---

## Validation Sign-Off

- [x] All requirements have manual-only verification (appropriate for OBS plugin)
- [x] Build-clean validation after every commit (automated gate)
- [x] Wave 0 not needed — no test framework expected
- [x] Feedback latency < 15s (build time)
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-04-06
