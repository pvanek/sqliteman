#!/bin/sh
# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds Sqliteman.app with every library it needs inside it, and a disk
# image of it. Run from anywhere on a Mac with Homebrew; see
# docs/development/packaging.md.
#
# Environment:
#   SQLITEMAN_SIGN_IDENTITY  "Developer ID Application: ..." to sign for
#                            distribution; without it the bundle is signed
#                            ad hoc, which runs only where it was built.
#   SQLITEMAN_NOTARY_PROFILE a notarytool keychain profile; with it (and an
#                            identity) the disk image is notarised and stapled.
#   SQLITEMAN_WITH_DOCS      OFF to leave the manual out (default ON).
set -eu

source_dir=$(cd "$(dirname "$0")/../.." && pwd)
work=${SQLITEMAN_WORK_DIR:-$source_dir/build-macos}
version=$(sed -n 's/^ *VERSION \([0-9.]*\)$/\1/p' "$source_dir/CMakeLists.txt" | head -n 1)
with_docs=${SQLITEMAN_WITH_DOCS:-ON}

echo "== Dependencies"
# Only the Qt modules the program uses. Homebrew's "qt" is every module
# there is, and macdeployqt would bundle the plugins of all of them (PDF,
# the virtual keyboard, ...). Some still come along: see below.
brew install cmake ninja qtbase qttools
brew_prefix=$(brew --prefix)

if [ "$with_docs" = ON ]; then
    python3 -m venv "$work/venv"
    "$work/venv/bin/pip" install --quiet -r "$source_dir/docs/requirements.txt"
    export PATH="$work/venv/bin:$PATH"
fi

echo "== Building $version"
# SQLite and QScintilla are built into the program from pinned archives:
# macOS's own SQLite has no column metadata, and nothing provides QScintilla.
cmake -S "$source_dir" -B "$work/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$brew_prefix" \
    -DSQLITEMAN_BUNDLED_SQLITE=ON \
    -DSQLITEMAN_BUNDLED_QSCINTILLA=ON \
    -DSQLITEMAN_BUILD_TESTS=OFF \
    -DSQLITEMAN_WITH_DOCS="$with_docs"
cmake --build "$work/build"
if [ "$with_docs" = ON ]; then
    cmake --build "$work/build" --target docs-qch
fi

rm -rf "$work/stage"
cmake --install "$work/build" --prefix "$work/stage"
app="$work/stage/sqliteman.app"

echo "== Bundling libraries"
# Copies the Qt frameworks and plugins, and every other non-system library
# the program reaches, into Contents/Frameworks, and points the program at
# those copies.
macdeployqt "$app" -verbose=1

# Homebrew keeps the plugins of all its Qt modules in one directory, and
# macdeployqt takes them all, but not the frameworks of modules the program
# does not use: the SVG plugins come with qttools, through qtdeclarative,
# without QtSvg. Such a plugin could never load; leave it out.
find "$app/Contents/PlugIns" -name '*.dylib' | while read -r plugin; do
    for framework in $(otool -L "$plugin" | awk '$1 ~ "^@rpath/Qt[^/]*[.]framework/" {
            sub("^@rpath/", "", $1); sub("/.*", "", $1); print $1 }'); do
        if [ ! -e "$app/Contents/Frameworks/$framework" ]; then
            echo "Leaving out ${plugin#"$app/Contents/PlugIns/"}: it needs $framework"
            rm "$plugin"
            break
        fi
    done
done

# macdeployqt misses a library now and then that is reached only through
# another one, such as brotli through FreeType. Copy whatever still comes
# from Homebrew into the bundle and point the references there; the check
# below has the last word. The new name is @rpath/..., which resolves to
# Contents/Frameworks: a reference can only be replaced by one no longer
# than itself, and @executable_path/../Frameworks/... often is longer.
frameworks="$app/Contents/Frameworks"
# Lines of "binary<tab>library" for every library taken from Homebrew.
homebrew_refs() {
    find "$app/Contents" -type f \( -perm -u+x -o -name '*.dylib' \) -print0 \
        | xargs -0 otool -L 2>/dev/null \
        | awk '/^[^\t].*:$/ { binary = substr($0, 1, length($0) - 1); next }
               $1 ~ "^(/opt/homebrew|/usr/local)/" { print binary "\t" $1 }'
}
for _ in 1 2 3 4 5; do
    refs=$(homebrew_refs)
    [ -z "$refs" ] && break
    printf '%s\n' "$refs" | while IFS="$(printf '\t')" read -r binary library; do
        name=$(basename "$library")
        if [ ! -e "$frameworks/$name" ]; then
            echo "Bundling $library"
            cp "$library" "$frameworks/$name"
            chmod u+w "$frameworks/$name"
            install_name_tool -id "@rpath/$name" "$frameworks/$name"
        fi
        if [ "$(basename "$binary")" = "$name" ]; then
            # A copy keeps its own name, which is where it came from.
            echo "Renaming the bundled $name"
            install_name_tool -id "@rpath/$name" "$binary"
        else
            echo "Pointing $(basename "$binary") at the bundled $name"
            install_name_tool -change "$library" "@rpath/$name" "$binary"
        fi
    done
done

# Anything still referring to Homebrew would work here and nowhere else.
leaks=$(find "$app/Contents" -type f \( -perm -u+x -o -name '*.dylib' \) -print0 \
    | xargs -0 otool -L 2>/dev/null \
    | grep -E '^[[:space:]]+(/opt/homebrew|/usr/local)/' || true)
if [ -n "$leaks" ]; then
    echo "The bundle still links outside itself:" >&2
    echo "$leaks" >&2
    exit 1
fi

echo "== Signing"
if [ -n "${SQLITEMAN_SIGN_IDENTITY:-}" ]; then
    codesign --force --deep --timestamp --options runtime \
        --sign "$SQLITEMAN_SIGN_IDENTITY" "$app"
else
    # macdeployqt rewrites the binaries; Apple silicon refuses to run
    # them unsigned.
    codesign --force --deep --sign - "$app"
fi
codesign --verify --deep --strict "$app"

echo "== Disk image"
dmg="$work/Sqliteman-$version.dmg"
image="$work/image"
rm -rf "$image" "$dmg"
mkdir "$image"
cp -R "$app" "$image/Sqliteman.app"
ln -s /Applications "$image/Applications"
hdiutil create -volname "Sqliteman $version" -srcfolder "$image" -fs HFS+ \
    -format UDZO -ov "$dmg"
rm -rf "$image"

if [ -n "${SQLITEMAN_SIGN_IDENTITY:-}" ]; then
    codesign --force --timestamp --sign "$SQLITEMAN_SIGN_IDENTITY" "$dmg"
    if [ -n "${SQLITEMAN_NOTARY_PROFILE:-}" ]; then
        xcrun notarytool submit "$dmg" --keychain-profile "$SQLITEMAN_NOTARY_PROFILE" --wait
        xcrun stapler staple "$dmg"
    fi
fi

echo "== Done: $dmg"
