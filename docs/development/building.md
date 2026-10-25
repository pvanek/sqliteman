# Building

```{index} building, dependencies
```

## What is needed

- CMake 3.25 and a C++20 compiler
- Qt 6.5 or later: Core, Gui, Widgets, Concurrent, and Test for the tests
- [QScintilla](https://riverbankcomputing.com/software/qscintilla) 2.13 or
  later for Qt 6
- SQLite 3.38 or later, built with `SQLITE_ENABLE_COLUMN_METADATA`
- Optional: Qt's Help module, to show this manual in the program
- Sphinx with the MyST parser for the manual, which is on by default;
  without it, configure with `-DSQLITEMAN_WITH_DOCS=OFF`

On openSUSE Tumbleweed:

```sh
sudo zypper install cmake gcc-c++ qt6-base-devel qt6-help-devel \
    qscintilla-qt6-devel sqlite3-devel
```

QScintilla and SQLite each come from the system when it has them, and are
otherwise downloaded and built as part of the project, at a pinned version
whose checksum the build verifies. `-DSQLITEMAN_BUNDLED_QSCINTILLA=ON` and
`-DSQLITEMAN_BUNDLED_SQLITE=ON` choose the bundled copy even when there is
a system one — the bundled SQLite has FTS5, R*Tree, Geopoly, the math
functions and `dbstat` compiled in. For an offline build, point
`FETCHCONTENT_SOURCE_DIR_QSCINTILLA` or `FETCHCONTENT_SOURCE_DIR_SQLITE_AMALGAMATION` at
an unpacked copy.

## Building and testing

```{index} tests
```

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/app/sqliteman
```

The tests need no server and no display: each makes the databases it needs
in a temporary directory, and the widget tests run on Qt's `offscreen`
platform with a settings directory of their own, so your preferences
neither change what they see nor are changed by them.

`-DSQLITEMAN_WERROR=ON` makes compiler warnings errors; the code builds
without any with GCC at `-Wall -Wextra -Wpedantic -Wshadow
-Wconversion`.

## Continuous integration

```{index} CI, continuous integration, GitHub Actions
```

`.github/workflows/ci.yml` builds and tests every push to `master` and
every pull request:

- on Fedora, with its own Qt, QScintilla and SQLite, as a distribution
  builds the program, with warnings as errors and the desktop and AppStream
  files validated;
- on Linux, macOS and Windows with Qt 6.8 from qt.io and the bundled
  QScintilla and SQLite, as a build on a machine without them does —
  warnings are errors on Linux;
- and the manual, the way the website is built, with warnings as errors.

## Building this manual

```{index} building the documentation, Sphinx, help file
```

The manual is built twice from the same Markdown sources: into a website,
and into the Qt help file the program shows itself.

```sh
sudo zypper install python313-Sphinx python313-myst-parser python313-furo
cmake -S . -B build -DSQLITEMAN_WITH_DOCS=ON
cmake --build build --target docs           # both of the below
cmake --build build --target docs-html      # build/docs/html
cmake --build build --target docs-qch       # build/docs/sqliteman.qch
```

`docs-qch` needs `qhelpgenerator`, which comes with Qt's tools, and copies
the help file next to the program so a build directory run finds it;
`cmake --install` puts it in `share/sqliteman`. For the program to show
it, Qt's Help module has to be there when the program is built; without it
the Help menu opens the website instead.

Sphinx can also be run directly, once the icon is in place:

```sh
cp src/app/icons/sqliteman-128.png docs/images/sqliteman.png
sphinx-build -b html docs build/docs/html
```

The website is published on GitHub Pages, at
<https://pvanek.github.io/sqliteman/>, by `.github/workflows/pages.yml`:
whenever the manual changes on `master`, it is built the same way, with
warnings treated as errors, and deployed. For that, the repository's Pages
source has to be set to *GitHub Actions*, once.

`docs/images/sqliteman.png` is the one generated file: the manual shows the
program's icon, and Sphinx takes images only from its own directory, so it
is copied there from `src/app/icons` by the documentation build and by
`src/app/icons/generate.sh`. It is not in the repository.

## Screenshots

```{index} screenshots
```

The pictures in the manual are taken by the program itself, not by hand:
`tools/screenshots` drives the real main window against a small sample
database, `tools/screenshots/shop.sql`, and saves each page into
`docs/images`. After a change to how something looks, take them again:

```sh
cmake -S . -B build -DSQLITEMAN_SCREENSHOTS=ON
tools/screenshots/run.sh build
```

It needs `Xvfb`, so the windows open on a display of their own, and keeps
its settings in a temporary directory: your own are neither shown nor
touched. `SQLITEMAN_STYLE` picks the widget style, Breeze by default.

## The icon

```{index} icon, logo
```

`src/app/icons/sqliteman.svg` is the program's icon, the symbol of the
logo Anouk Voogt designed for Sqliteman; the full logo, with the wordmark,
is in `logo_design/`. `src/app/icons/generate.sh` renders the PNGs of every
size and the Windows `.ico` from the SVG, with Inkscape and ImageMagick.
