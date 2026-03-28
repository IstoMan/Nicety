---
name: e2e-test
description: >-
  End-to-end validation for the nicety codebase: CMake build, run the SDL3
  PDF viewer, and exercise native UI flows (open PDF, scroll, view mode,
  resize). Uses parallel research plus manual or screenshot-based checks — no
  browser or database. Use when the user asks for E2E, smoke tests, UI
  validation, or pre-review checks for nicety.
---

# End-to-End Testing (nicety)

**nicety** is a native **C11** app: **SDL3**, **Clay** UI, **MuPDF**, **SDL3_ttf** / **SDL3_image**. There is **no** web frontend, **no** `package.json` dev server, and **no** database. Do **not** use agent-browser or browser E2E tools for this project.

## Stack reference (quick)

| Area | Details |
|------|---------|
| Build | `CMakeLists.txt` → executable **`nicety`** |
| UI | Clay layouts in `src/ui.c`; app state in `src/app.c` |
| PDF | `src/document_*.c` (layout, raster, SDL upload, sync/CPU page window), MuPDF via `pkg_check_modules(MUPDF …)` |
| File pick | `tinyfiledialogs` (`SDL_EVENT_MOUSE_BUTTON_DOWN` → open dialog) |
| Test PDF | `resources/book.pdf` (repo); dialog defaults start under `./resources/` |

## Pre-flight

### 1. Platform

Target **Linux** or **macOS** with a **display** (Wayland or X11 on Linux). Running the GUI **requires** `DISPLAY` / Wayland session for interactive tests.

- If there is no display (SSH without forwarding, CI without GPU/display): limit validation to **compile-only** / **unit-style** checks and state that full UI E2E needs a local graphical session.

### 2. Dependencies

Build needs **CMake**, **SDL3**, **SDL3_image**, **SDL3_ttf**, and **MuPDF** development packages (e.g. `pkg-config` finds `mupdf`). If configure fails, document missing packages; do not assume npm or a web stack.

### 3. Build smoke test

From the repo root:

```bash
cmake -S . -B build
cmake --build build
```

Expect binary at **`build/nicety`** (or `build/Debug/nicety` on some multi-config generators — locate the produced executable).

Run:

```bash
./build/nicety
```

(Adjust path if the generator places the binary elsewhere.)

## Phase 1: Parallel research

Launch **three** sub-agents in parallel via the Task tool.

### Sub-agent 1: App structure & user journeys

Research the repo and return:

1. **How to build and run** — exact `cmake` / build commands and path to the `nicety` binary.
2. **Window** — title (`Nicety` in `main.c`), default size (~1200×800), resizable.
3. **App states** (`AppState` in `app.h`): **`LOAD_FILE`** vs **`FILE_VIEW`**.
4. **Every user journey** — at minimum, map to code:
   - **Load screen**: “Click to Select” / “or Drop a File” (`ui_load_file_layout`).
   - **Open via dialog**: left-click → `tinyfd_openFileDialog`, `.pdf` only (`path_has_pdf_extension`), success → `FILE_VIEW`.
   - **Open via drop**: `SDL_EVENT_DROP_FILE` → same PDF rules.
   - **Document view**: header **View mode** button toggles `VIEW_MODE_FILL` ↔ `VIEW_MODE_FIT_HEIGHT` (`ViewModeBtn`, `toggle_view_mode`).
   - **Scroll**: mouse wheel → `Clay_UpdateScrollContainers` (sidebar + content); document may virtualize pages (`document_load_page_window`, scroll sync in `app_on_update`).
   - **Resize**: window resize → Clay layout sync (`app_sync_clay_layout_to_renderer`).
5. **Key UI regions** — sidebar thumbnails (`Sidebar`, `DocSidebarPage`), content area, header (`Header` / `ViewModeBtn`). Testing should cover interactions that hit these.

### Sub-agent 2: Data layer (no SQL)

There is **no** database. Instead summarize:

1. **In-memory / file state**: `Document`, `DocumentContext`, `Document::file_path`, page windowing (`NICETY_PAGE_WINDOW_RADIUS`), arenas.
2. **What mutates on actions**: opening/closing PDF, scroll-driven page loads, view mode changes (layout only).
3. **“Validation” substitutes**: stderr messages (e.g. `Couldn't load PDF`, `Please select a .pdf file`), visible UI state (mode label “Mode: Fill” vs “Mode: Fit”), and that scrolling loads pages without repeated `document_load_page_window` failures (watch stderr).

### Sub-agent 3: Bug hunt

Same focus as before: logic errors, UI/state bugs, resource leaks, security (path handling, untrusted PDFs). **Prioritize** `src/app.c`, `src/document_*.c`, `src/ui.c`, `src/core.c`.

Wait for all three sub-agents before Phase 2.

## Phase 2: Run the application

1. Build if needed (Phase 1 should confirm commands).
2. Start **`nicety`** in the **foreground** for interactive manual pass, or document that the user should run it locally if there is no display.
3. Optional: capture a baseline screenshot after launch (see Phase 4 tools) — e.g. `e2e-screenshots/00-load-screen.png`.

## Phase 3: Todo list

Use **TodoWrite**: one todo per user journey from Sub-agent 1 (open via dialog, open via drop, toggle view mode, scroll sidebar/content, resize window), plus a final todo for **window resize / layout** sanity.

## Phase 4: Journey validation (native)

**Not** agent-browser. Use one or more of:

1. **Manual** — Follow journeys in Sub-agent 1; confirm visually and via stderr.
2. **Screenshots** (when a display is available) — save under `e2e-screenshots/<journey>/…`. Examples:
   - **Linux Wayland**: `grim path.png`
   - **X11**: `import -window root path.png` (ImageMagick) or `maim path.png`
   - **macOS**: `screencapture -x path.png`
3. **Read** screenshot images with the Read tool to check layout and obvious glitches.

Per journey: note **steps**, **expected UI** (strings from Clay, mode label), and **stderr** expectations.

### Issue handling

On failure: capture stderr, screenshot path if any, suspected file/line from Sub-agent 3; fix code; rebuild; re-run the same journey.

## Phase 5: Cleanup

Close the app window; no dev server to kill unless you started the binary in the background.

## Phase 6: Report

Output a concise summary:

```text
## E2E / smoke testing complete (nicety)

**Journeys exercised:** [count]
**Screenshots:** [count] (or "none — no display")
**Issues found:** [count] ([count] fixed, [count] remaining)

### Build
- [OK / FAILED] cmake + build

### Issues fixed during testing
- [Description] — [file:line]

### Remaining issues
- [Description] — [severity] — [file:line]

### Bug hunt (code review)
- [Description] — [severity] — [file:line]

### Artifacts
Screenshots (if any): `e2e-screenshots/`
```

### Markdown export (ask first)

Ask whether to write **`e2e-test-report.md`** in the project root with full journey notes, screenshot paths, stderr snippets, and unresolved items — only if the user wants it.

## Original skill

The generic web/agent-browser workflow was adapted from [coleam00/link-in-bio-page-builder](https://github.com/coleam00/link-in-bio-page-builder) `.claude/skills/e2e-test/SKILL.md` for **nicety**’s native PDF viewer architecture.
