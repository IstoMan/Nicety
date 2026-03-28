---
name: session-perf-reports
description: >-
  Guides session-based performance capture for native apps (run until the user
  closes the app), aggregation of artifacts after exit, and generation of
  structured perf reports for agents and LLMs. Covers external profilers (perf,
  sampling), optional in-app event logs, and a fixed report layout so models can
  compare runs and extract insights. Use when the user asks for perf reports,
  profiling sessions, performance analysis after interactive use, or
  LLM-readable performance documentation for nicety or similar C/SDL binaries.
---

# Session performance reports (nicety)

**Goal:** Capture performance data during **real interactive use**, treat **normal application exit** as the end of the session, then produce **one primary document** that an agent (or human) can read without raw binary artifacts.

**nicety** is a **C11 / SDL3 / MuPDF** GUI (`build/nicety`). There is no built-in perf server; prefer **OS-level tools** and optional **NDJSON logging** if the codebase adds it.

## Principles

1. **Session boundary** — Start capture → user exercises the app → user quits cleanly (`SDL_EVENT_QUIT` / window close). Do not assume a fixed time limit; duration is implicit.
2. **Non-intrusive collection** — Prefer tools that record in the background. Avoid pausing the UI mid-session to “generate a report.”
3. **Post-exit synthesis** — After exit, run `perf report`, parse logs, or merge traces; then write the structured report.
4. **LLM-oriented output** — Use the template below: explicit units, file paths to raw data, reproduction steps, and **actionable** bullet points (not prose-only narrative).

## Capture methods

Pick one primary method per session; note it in the report under **Methodology**.

### A. Linux `perf` (CPU-oriented)

From repo root, with a **RelWithDebInfo** or **Debug** build (`-g` is already in `CMakeLists.txt`):

```bash
perf record -g -F 997 --call-graph dwarf -o /tmp/nicety-perf.data -- ./build/nicety
```

User closes the app → `perf.data` is complete. Then:

```bash
perf report -i /tmp/nicety-perf.data --stdio > /tmp/nicety-perf-report.txt
# optional: perf script for flame graph tools later
```

**Limits:** GPU/display work may not dominate CPU stacks; say so in the report if relevant.

### B. Wall-clock and coarse stats

```bash
/usr/bin/time -v ./build/nicety 2> /tmp/nicety-time.txt
```

Useful for **max RSS**, **minor/major faults**, and total wall time for the whole session. Pair with (A) or in-app counters.

### C. In-app event log (if implemented)

If the code writes **NDJSON** lines (one JSON object per line) to a file, open with `O_APPEND`, **fflush on exit** (atexit or `SDL_EVENT_QUIT` handler). The skill consumer should:

- Keep **schema stable** (e.g. `{"t_ms":..., "event":"frame", ...}`).
- After exit, **validate JSONL** (`jq -c . file` or a tiny script) before summarizing.

## Workflow checklist

```
- [ ] Rebuild if symbols changed: cmake --build build
- [ ] Choose capture method (perf / time / in-app log / combination)
- [ ] Start capture; user performs representative actions (open PDF, scroll, resize, view modes — see e2e-test skill)
- [ ] User exits app normally; verify artifacts exist and sizes are non-zero
- [ ] Run post-processing (perf report, jq, etc.)
- [ ] Write the session report using the template below; link or copy raw paths
```

## LLM-oriented report template

Use this structure verbatim (fill sections; omit only if truly N/A):

```markdown
---
kind: perf-session
app: nicety
date: YYYY-MM-DD
session_id: short-unique-id
commit: <git rev-parse --short HEAD>
capture: perf-record | time-v | ndjson | other
---

# Performance session: <one-line summary>

## Executive summary
- **Primary finding:** …
- **Severity / impact:** … (e.g. UI jank, RSS growth, hot C functions)
- **Confidence:** high | medium | low (and why)

## Environment
- **OS / kernel:** …
- **CPU / GPU (if noted):** …
- **Build:** Debug | RelWithDebInfo | Release, relevant `CMAKE_*` flags
- **Input:** which PDF(s), approximate session length, user actions performed

## Methodology
- **Tools:** exact commands run (copy from shell history)
- **Session end:** normal quit (yes/no)
- **Raw artifacts:** full paths to `perf.data`, `perf-report.txt`, `time.txt`, logs, etc.

## Metrics (quantitative)

| Metric | Value | Source |
|--------|-------|--------|
| Wall time (s) | | time / perf |
| Max RSS (KB) | | time -v / proc |
| Samples / events | | perf / log lines |

Add domain rows as needed (e.g. frames logged, page loads).

## Hotspots and interpretation
- **Top stacks / symbols:** bullet list with **percent or sample share** where available
- **Likely causes:** short hypotheses tied to code areas (file:function if known)
- **What this is not:** e.g. “does not measure GPU frame time unless …”

## Anomalies and gaps
- Missing symbols, truncated stacks, single-run variance, untested code paths
- **Follow-up experiments:** specific next commands or code probes

## Reproduction
1. Checkout `commit`
2. Build: `cmake -S . -B build && cmake --build build`
3. Run: `<exact command>`
4. Actions: `<ordered list>`
5. Exit: `<how>`

## Appendix: raw excerpts
Paste only **short** excerpts from `perf report` or logs; keep large blobs as file references.
```

## What the agent should produce

When asked to “analyze perf” or “turn this run into a report”:

1. Confirm **session ended cleanly** and artifacts match the claimed tool.
2. Populate **Metrics** and **Hotspots** from data, not from generic SDL advice.
3. Call out **uncertainty** (single run, turbo, thermal, debug build overhead).
4. Prefer **paths and numbers** over adjectives so another LLM turn can diff two reports.

## Related

- Interactive UI coverage: project skill **e2e-test** (`.cursor/skills/e2e-test/SKILL.md`).
- If the user adds Tracy, eBPF, or custom timers, extend **Methodology** and the metrics table—keep one report template for comparability.
