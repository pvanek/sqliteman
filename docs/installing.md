# Installing

```{index} installing, packages
```

## Ready-made packages

```{index} downloads, Windows, macOS, Flatpak
```

Each [release](https://github.com/pvanek/sqliteman/releases) on GitHub has
packages with everything the program needs inside:

- **Windows** (64-bit, Windows 10 or later): `Sqliteman-<version>-setup.exe`
  installs it, for you alone without administrator rights or for everyone;
  `Sqliteman-<version>-win64.zip` runs from wherever it is unpacked.
- **macOS**: `Sqliteman-<version>-macos-arm64.dmg` for Apple silicon,
  `-macos-x86_64.dmg` for Intel. Drag Sqliteman into Applications.
- **Linux**: `Sqliteman-<version>.flatpak`, installed with
  `flatpak install --user Sqliteman-<version>.flatpak`, gets its runtime
  from Flathub. It sees your home directory, where it can open databases.

`SHA256SUMS` beside them lists their checksums. Not every release has
every package.

## From your distribution

```{index} Linux packages
```

Many Linux distributions have packaged Sqliteman for years. A package built
from version 2 or later is this program; one at 1.2 or older is the Qt 4
original, which is no longer maintained.

## From source

```{index} source code
```

The source is on GitHub,
[github.com/pvanek/sqliteman](https://github.com/pvanek/sqliteman). It
builds with CMake on Linux, macOS and Windows; see
[building](development/building.md) for what it needs and how.

In short:

```sh
git clone https://github.com/pvanek/sqliteman.git
cmake -S sqliteman -B build
cmake --build build
cmake --install build          # or run build/src/app/sqliteman
```

## Which SQLite

```{index} SQLite version, bundled SQLite
```

Sqliteman needs SQLite 3.38 or later, built with column metadata, as most
distributions build it. When the system has no such SQLite, or when asked
to with `-DSQLITEMAN_BUNDLED_SQLITE=ON`, the build downloads a pinned
SQLite and puts it inside the program. That copy comes with full-text
search (FTS4 and FTS5), R*Tree and Geopoly, the math functions and the
`dbstat` table switched on.

The status bar of the main window says which SQLite the program runs on.
