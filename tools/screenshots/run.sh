#!/bin/sh
# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

# Retakes the manual's screenshots into docs/images. Needs Xvfb and a build
# directory configured with -DSQLITEMAN_SCREENSHOTS=ON:
#
#   tools/screenshots/run.sh [build directory]
#
# Everything runs on a display and a settings directory of its own, so
# neither the desktop nor the real settings are touched.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
source_dir=$(cd "$here/../.." && pwd)
build_dir=$(cd "${1:-$source_dir/build}" && pwd)
display=${SQLITEMAN_SCREENSHOTS_DISPLAY:-:97}
work=$(mktemp -d)

cleanup() {
    [ -n "${xvfb:-}" ] && kill "$xvfb" 2>/dev/null || true
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

cmake --build "$build_dir" --target sqliteman_screenshots

Xvfb "$display" -screen 0 1600x1000x24 -nolisten tcp >/dev/null 2>&1 &
xvfb=$!
sleep 1

DISPLAY=$display WAYLAND_DISPLAY= QT_QPA_PLATFORM=xcb QT_STYLE_OVERRIDE=${SQLITEMAN_STYLE:-Breeze} \
    XDG_CONFIG_HOME="$work" DBUS_SESSION_BUS_ADDRESS=disabled: \
    "$build_dir/tools/screenshots/sqliteman_screenshots" \
    "$here/shop.sql" "$work" "$source_dir/docs/images"
