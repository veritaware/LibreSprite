# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Besprited is a free/open-source 2D sprite editor and animation tool, written in C++23. It is a fork of
[LibreSprite](https://github.com/LibreSprite/LibreSprite) (itself a GPLv2 fork of the last open-source
Aseprite), maintained largely as a single-developer opinionated fork. Scripting is powered by an embedded
QuickJS-ng engine (vendored as a git submodule).

## Build commands

The source must be cloned recursively (submodules under `third_party/`):

```
git clone --recursive https://github.com/Veritaware/Besprited
git submodule update --init --recursive   # if already cloned
```

Standard out-of-tree CMake + Ninja build:

```
mkdir build && cd build
cmake -G Ninja -DENABLE_TESTS=on ..
ninja besprited
```

- `besprited` is the main target (also aliased as `libresprite` for backward compatibility).
- Build type defaults to `RelWithDebInfo`; other types: `Debug`, `Release`, `Profile` (a custom profiling
  build type set up in the root `CMakeLists.txt`).
- Key CMake options (see root `CMakeLists.txt`): `ENABLE_TESTS`, `WITH_WEBP_SUPPORT`,
  `WITH_GTK_FILE_DIALOG_SUPPORT`, `WITH_DESKTOP_INTEGRATION`, `USE_SDL2_BACKEND` (on by default),
  `ENABLE_MEMLEAK`, `FULLSCREEN_PLATFORM`.
- Install with `ninja install` from the `build` directory.
- Full platform dependency lists (Linux/Windows/macOS/Android) are in `INSTALL.md`.

### Static analysis

From the `build` directory:

```
cmake --build . --target cppcheck    # requires cppcheck
cmake --build . --target clang-tidy  # requires clang-tidy
```

### Tests

Unit tests live in the top-level `test/` directory (mirroring the `src/` module layout) and must be
enabled via `-DENABLE_TESTS=on` at configure time. They use GoogleTest, provided by `test/CMakeLists.txt`
via `FetchContent` — a system-installed GTest is used when available, otherwise a pinned copy is fetched
at configure time (no submodule). Test discovery is automatic: any `*_tests.cpp` file inside a module's
`test/<module>/` directory (`base`, `gfx`, `doc`, `render`, `css`, `ui`, `app/file`, `app`) becomes its
own standalone test executable and CTest entry — see the `find_tests(...)` calls in `test/CMakeLists.txt`.

```
ninja                 # builds all *_tests executables too
ctest                 # run the full suite from the build dir
ctest -R <test_name>  # run a single test executable (name == the *_tests.cpp basename)
./bin/some_tests       # or run the built test binary directly
```

A few modules (`src/clip`, `src/undo`, and the vendored `third_party/observable`) keep their own
self-contained `tests/CMakeLists.txt` (with a local `add_<module>_test()` helper and a tiny in-tree
test header) rather than the GoogleTest suite under `test/`.

## Architecture

The codebase is organized under `src/` as a strict dependency-layered set of modules — each module only
depends on modules below it. This is documented in detail per-module in `src/<module>/README.md`; read
those before making non-trivial changes to a module you're unfamiliar with. Summary (see `src/README.md`
for the canonical version):

- **Level 0 (no internal deps):** `base` (utf8, sha1, fs, threading, memory primitives), `clip`
  (clipboard), `css` (a pseudo-stylesheet library used for skinning), `fixmath` (fixed-point math), `flic`
  (FLI/FLC loader), `gfx` (point/size/rect/region/color primitives), `script` (the QuickJS-based scripting
  engine), `undo` (generic undoable-command history), `wacom` (Wintab tablet definitions).
- **Level 1:** `cfg` (INI-style settings load/save, depends on `base`), `gen` (depends on `base`; a
  build-time code generator, see below).
- **Level 2:** `doc` (the document/sprite data model; depends on `base`, `fixmath`, `gfx`), `she`
  (platform abstraction layer over SDL2; depends on `base`, `gfx`, `wacom`).
- **Level 3:** `filters` (image effects), `render` (renders documents to bitmaps), `ui` (the portable
  widget toolkit — buttons, windows, text fields — built on `she`).
- **Level 4:** `app` — the actual application: commands (`app/commands/cmd_*.cpp`, one file per editor
  command/menu action), editors, UI screens (`app/ui/`), file format codecs, etc. This is where most
  feature work happens.
- **Level 5:** `main` — process entry point wiring `app` together.

### Code generation (`gen`)

`src/gen` is a build-time tool that turns XML into generated C++ so things are checked at compile time
rather than parsed at runtime. It generates from three families of XML input:

1. Widget subclasses from `data/widgets/*.xml` (so widget trees defined in XML get typed C++ accessors).
2. Config wrappers from a `config-metadata.xml`-style file, replacing manual
   `get/set_config_int/bool/string()` calls.
3. Theme-data accessor classes from `data/skins/default/` (colors, styles, slices, etc.).

If you're adding a new widget/dialog defined in `data/widgets/` or a new theme slice, expect a
corresponding generated accessor rather than hand-writing lookup code.

### Scripting

Scripting API is documented in `SCRIPTING.md`. The engine itself lives in `src/script/` (QuickJS-ng
vendored under `src/script/quickjs` as a submodule) and exposes app objects (Sprite, Layer, Storage, etc.)
as JS classes/globals; user-facing scripts live under `data/scripts/`.

## Contribution conventions

- Branches: `trunk` is the primary development branch (this repo's default). `ls-develop` tracks upstream
  LibreSprite's `master` for periodic merges; feature/fix branches meant to be shareable with LibreSprite
  are usually based on `ls-develop`. Feature/PR branches should be named
  `username/short-description`.
- Commit messages: imperative mood ("Fix bug", not "Fixed bug"), subject line ≤ 50 chars, reference closed
  issues (e.g. "Fixes #123"). For non-source changes (docs, workflows, issue templates, etc.), add a
  trailing `NO_SW_CHANGE` line to skip GitHub build workflows.
- AI-assisted contributions are allowed but must be reviewed/understood by the human submitting them, and
  must be marked as AI-co-authored in the commit (e.g. `Co-authored-by: Claude <noreply@anthropic.com>`).
  See `AI_USAGE.md` for the full policy — unreviewed/low-quality AI output is grounds for immediate PR
  rejection.
- **Copyright headers:** for every `.h`/`.hpp`/`.c`/`.cpp`/`.xml`/`CMakeLists.txt` file you modify, check
  the copyright header in the first ~10 lines and update/add the Besprited/Veritaware line to cover the
  current year, e.g. `// Besprited | Copyright (C) 2026 Veritaware` (check `src/app/app.cpp`, `data/gui.xml`,
  `src/CMakeLists.txt` for formatting examples). For exempt files list check `.github/copyright-exempt.txt`.
