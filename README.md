# Nicety

Native PDF viewer for Linux, built with **SDL3**, **[Clay](https://github.com/nicbarker/clay)** for layout, and **MuPDF** for rendering. The interface uses a **Catppuccin Macchiato** palette ([`src/colors.h`](src/colors.h)).

## Features

- Open PDFs via the **Open** button, **drag-and-drop**, or the initial empty screen (click to pick a file).
- **Scroll** the main page column and the thumbnail sidebar; wheel sensitivity is configurable in code (`App.sensitivity`).
- **View mode** toggles between filling the content width and fitting the page height (toolbar **Mode** control).
- Sidebar hides automatically when the window is too narrow; layout tracks window size every frame.

## Requirements

- **CMake** 3.10+
- **C11** compiler
- **SDL3**, **SDL3_image**, **SDL3_ttf** (CMake packages `SDL3`, `SDL3_image`, `SDL3_ttf`)
- **MuPDF** (`pkg-config` module `mupdf`)

On Arch Linux, for example: `sdl3`, `sdl3_image`, `sdl3_ttf`, `mupdf`.

Runtime assets live under **`res/`** (font, toolbar icons). Run the binary from the **repository root** so paths like `res/interface.ttf` and `res/icon_*.png` resolve, or run from a directory that contains a `res` folder with those files.

## Build

```bash
cmake -S . -B build
cmake --build build
```

The executable is `build/nicety` (or `build/nicety.exe` on Windows).

## Usage

| Action | Behavior |
|--------|----------|
| Click empty screen / **Open** | System file dialog (PDF filter) |
| Drop `.pdf` onto the window | Open that file |
| **Mode** button | Switch Fill ↔ Fit height |
| Mouse wheel | Scroll active panes |

Close the window to quit.

## Repository layout

| Path | Role |
|------|------|
| `src/` | Application, PDF document pipeline, UI (`ui.c`), theme (`colors.h`) |
| `vendor/` | Clay single-header and SDL3 Clay renderer |
| `res/` | Font and UI bitmaps |

## Third-party

Vendored libraries (for example Clay under `vendor/`) remain under their original licenses.
