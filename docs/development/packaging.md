# Packaging

```{index} packaging, releases, Flatpak, Windows installer, macOS disk image
```

`tools/` has one directory per package, each with a script that builds it
from a checkout:

| Directory  | What comes out                                 |
|------------|------------------------------------------------|
| `macos/`   | `Sqliteman.app` on a disk image                |
| `windows/` | A portable zip and an Inno Setup installer     |
| `flatpak/` | A Flatpak manifest, and a single-file bundle   |

What more than one of them needs stays where the build installs it from:
the desktop file and the AppStream metainfo, named after the application
ID `cz.yarpen.sqliteman`, are in `src/app`. The version comes from
`project()` in `CMakeLists.txt`, as everywhere else.

Every package carries SQLite and QScintilla built into the program, from
the archives pinned in `cmake/`, and the manual as the Qt help file. The
scripts do not run the tests; run `ctest` in a normal build first.

## macOS

Needs Xcode's command line tools and [Homebrew](https://brew.sh); the
script installs Qt, CMake and Ninja itself, and Sphinx into a virtualenv.

```sh
tools/macos/build.sh        # build-macos/Sqliteman-<version>.dmg
```

`macdeployqt` copies Qt into the bundle; the script then copies whatever
still comes from Homebrew, and fails if anything in the bundle reaches
outside it. Without more, the bundle is signed ad hoc: it runs on the Mac
that built it, and anywhere else only after the user clears the quarantine.
To ship it, sign with a Developer ID and have Apple notarise it:

```sh
xcrun notarytool store-credentials sqliteman-notary   # once
SQLITEMAN_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)" \
SQLITEMAN_NOTARY_PROFILE=sqliteman-notary tools/macos/build.sh
```

Homebrew builds for the macOS it runs on, and so does this: the package
needs that version or later, and the processor of the build machine.
`SQLITEMAN_WITH_DOCS=OFF` leaves the manual out.

## Windows

Needs Visual Studio 2022 (or its Build Tools) with the C++ workload, Qt 6.5
or later for MSVC 64-bit, PowerShell 7.3 or later and Python 3; optionally
[Inno Setup 6](https://jrsoftware.org/isinfo.php) for the installer.

```powershell
tools\windows\build.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64
```

It enters the Visual Studio environment itself and assembles the program
in `build-windows\Sqliteman`: Qt by `windeployqt`, the C++ runtime beside
it so nothing needs installing first, and anything else by
`deploy-dlls.cmake`, which fails on a DLL it cannot find. Out come
`Sqliteman-<version>-win64.zip` and, with Inno Setup,
`Sqliteman-<version>-setup.exe`, which installs per user or for everyone.
`-NoDocs` leaves the manual out, and `-SignCertificate` signs the program
and the installer with `signtool`.

## Flatpak

Needs `flatpak` with the Flathub remote; the script installs
`flatpak-builder` and the KDE 6.11 SDK itself.

```sh
tools/flatpak/build.sh              # build-flatpak/Sqliteman-<version>.flatpak
tools/flatpak/build.sh --install    # and install it for this user
flatpak run cz.yarpen.sqliteman
```

Flatpak builds have no network, so the manifest lists the SQLite and
QScintilla archives with their hashes; when `cmake/` pins new ones, change
the manifest with it. The sandbox sees the home directory, as SQLite keeps
its journal beside the database. `python3-docs.yaml`, Sphinx for the
manual, is generated from `docs/requirements.txt` with
[flatpak-pip-generator](https://github.com/flatpak/flatpak-builder-tools/tree/master/pip):

```sh
cd tools/flatpak
flatpak-pip-generator --runtime org.kde.Sdk//6.11 --build-only --yaml \
    -r ../../docs/requirements.txt -o python3-docs
```

## Releases

`.github/workflows/release.yml` runs these scripts for every tag
`v<version>`: macOS on Apple silicon and on Intel, Windows, and the
Flatpak. The tag has to match `VERSION` in `CMakeLists.txt`, or nothing is
built. Each platform builds on its own: whatever built goes into the
release with a `SHA256SUMS` of it, and the run warns about the rest. A
failed platform can be fixed and its job re-run, and its package is added
to the same release.

The release is made as a draft, whose notes are written by hand before it
is published. If the tag already has a release, the packages are added to
it instead. Started by hand (*Run workflow*), the workflow builds the
packages from any branch and keeps them as the run's artifacts, without a
release.

```sh
version=$(sed -n 's/^ *VERSION \([0-9.]*\)$/\1/p' CMakeLists.txt | head -n 1)
git tag -a "v$version" -m "version $version"
git push origin "v$version"
```

Without further setup the macOS disk images are signed ad hoc and the
Windows program is not signed. For a Developer ID signature and
notarisation, set these repository secrets:

| Secret                       | What it holds                                            |
|------------------------------|----------------------------------------------------------|
| `MACOS_CERTIFICATE`          | The Developer ID Application certificate, `.p12`, base64 |
| `MACOS_CERTIFICATE_PASSWORD` | The `.p12`'s password                                    |
| `MACOS_SIGN_IDENTITY`        | `Developer ID Application: Name (TEAMID)`                |
| `MACOS_NOTARY_APPLE_ID`      | The Apple ID notarisation runs as                        |
| `MACOS_NOTARY_PASSWORD`      | An app-specific password of that Apple ID                |
| `MACOS_NOTARY_TEAM_ID`       | The team ID                                              |
