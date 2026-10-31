<h1 align="center">Globe</h1>

<p align="center">
  <img src="https://img.shields.io/badge/version-v0.1-f5a623" alt="Version v0.1">
  <img src="https://img.shields.io/badge/platform-Haiku-ffcc00" alt="Platform: Haiku">
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599c" alt="C++17">
</p>

Globe is a Haiku-native editor for web files. This repository is a careful
modernisation of the original BeOS-era Globe editor, whose source is preserved
in [`Globe-Original`](Globe-Original/).

The current development version provides a small, dependable editor core:

- native Haiku document windows;
- new, open, save, and save-as workflows;
- multiple documents in separate windows;
- standard editing commands and shortcuts;
- protection against accidentally closing an unsaved document;
- line numbers that remain aligned while editing and scrolling;
- a live line, column, file-type, and modification-status display;
- theme-aware HTML and PHP syntax highlighting while editing;
- previewing in Haiku's preferred browser or the Tororu document viewer;
- an About window preserving Globe's original credits, artwork, and licence;
- and a compact HTML tag toolbar with delayed tag completion after typing `<`.

## Building

Globe must be built on Haiku with the standard development tools installed:

```sh
make
```

Install it for the current user with:

```sh
make install
```

The default destination is `/boot/home/config/non-packaged/bin`. Override it
with `make install INSTALL_DIR=/another/path` when needed.

The portable syntax-highlighter and tag-template tests can be run on Haiku or
another system with a C++17 compiler:

```sh
make test
```

## Roadmap

Development is proceeding in small, testable milestones:

- [x] Add line numbers and a status bar showing the current line, column, file
  type, and modification state.
- [x] Add HTML and PHP syntax highlighting.
- [x] Add Preview commands for Haiku's preferred browser and Tororu.
- [x] Port the original tag toolbar and tag-completion menu.
- [ ] Add preferences, session restoration, and the original icon and
  resources.
- [ ] Package Globe as an `.hpkg`.

## Project layout

- `src/` contains the modern Haiku application.
- `Globe-Original/` is the historical source used as the behavioural and
  design reference. Changes for the modern application should be made in
  `src/`, leaving that reference tree intact.

