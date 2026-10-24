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
- standard editing commands and shortcuts; and
- protection against accidentally closing an unsaved document.

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

## Project layout

- `src/` contains the modern Haiku application.
- `Globe-Original/` is the historical source used as the behavioural and
  design reference. Changes for the modern application should be made in
  `src/`, leaving that reference tree intact.

