---
phase: 1
slug: plugin-scaffold-and-core-rendering
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-29
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None — C++ OBS plugin, no unit test framework for Phase 1 |
| **Config file** | None — Wave 0 creates build system |
| **Quick run command** | `cmake --build build_macos && echo "Build OK"` |
| **Full suite command** | Manual OBS load verification (see Manual-Only Verifications) |
| **Estimated runtime** | ~15 seconds (build), ~60 seconds (manual OBS load) |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build_macos && echo "Build OK"`
- **After every plan wave:** Full build + manual OBS load test
- **Before `/gsd:verify-work`:** All five success criteria verified manually in OBS
- **Max feedback latency:** 15 seconds (build)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| TBD | 01 | 1 | CORE-05 | build | `cmake --build build_macos` | ❌ W0 | ⬜ pending |
| TBD | 01 | 1 | CORE-01 | manual | OBS load + add source | ❌ W0 | ⬜ pending |
| TBD | 02 | 1 | CORE-02 | manual | Draw freehand in OBS preview | ❌ W0 | ⬜ pending |
| TBD | 02 | 1 | CORE-03 | manual | Concurrent draw/clear stress test | ❌ W0 | ⬜ pending |
| TBD | 02 | 1 | CORE-04 | grep | `grep -rn "gs_" src/ --include="*.cpp" --include="*.hpp"` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `CMakeLists.txt` — cmake 3.28+, ENABLE_QT=OFF, ENABLE_FRONTEND_API=OFF, OBS.app framework path
- [ ] `buildspec.json` — OBS 31.1.1 dependency hashes from obs-plugintemplate
- [ ] `src/plugin.cpp` — module entry with OBS_DECLARE_MODULE, obs_register_source
- [ ] `src/chalk-source.hpp` / `src/chalk-source.cpp` — obs_source_info struct, required callbacks stubbed
- [ ] `src/marks/mark.hpp` — abstract Mark base class
- [ ] `src/mark-list.hpp` / `src/mark-list.cpp` — MarkList with std::mutex

*No unit test framework required — build + OBS load is the validation method.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Plugin appears in OBS "Add Source" list | CORE-01 | Requires running OBS Studio | Launch OBS → Sources → + → verify "Chalk" appears → add → verify transparent overlay composites |
| Freehand stroke visible in preview | CORE-02 | Requires OBS preview interaction | Add Chalk source → move mouse in preview while drawing → verify stroke renders |
| Clear-all doesn't crash | CORE-03 | Requires OBS runtime + threading | Draw several marks → trigger clear → verify clean frame, no crash, no artifacts |
| No gs_* calls outside video_render | CORE-04 | Semi-automated via grep | `grep -rn "gs_" src/` — verify all matches are inside video_render or called from video_render |

*GPU threading safety (CORE-04) is primarily a code review gate, supplemented by grep.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
