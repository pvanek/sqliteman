#!/bin/sh
# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the Flatpak from this checkout into a single-file bundle, and with
# --install also installs it for the current user. Needs flatpak and the
# Flathub remote; flatpak-builder and the KDE SDK are installed if missing.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
source_dir=$(cd "$here/../.." && pwd)
work=${SQLITEMAN_WORK_DIR:-$source_dir/build-flatpak}
app_id=cz.yarpen.sqliteman
version=$(sed -n 's/^ *VERSION \([0-9.]*\)$/\1/p' "$source_dir/CMakeLists.txt" | head -n 1)

# flatpak-builder from Flathub, rather than the distribution's, so every
# machine builds with the same one.
flatpak --user remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak --user install -y --noninteractive flathub org.flatpak.Builder

install=
if [ "${1:-}" = --install ]; then
    install="--user --install"
fi

mkdir -p "$work"
# shellcheck disable=SC2086 # $install is a list of options or nothing
flatpak run --filesystem="$source_dir" --filesystem="$work" org.flatpak.Builder \
    --install-deps-from=flathub --user \
    --force-clean --ccache \
    --state-dir="$work/state" \
    --repo="$work/repo" \
    $install \
    "$work/app" "$here/$app_id.yml"

flatpak build-bundle "$work/repo" "$work/Sqliteman-$version.flatpak" "$app_id" \
    --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo
echo "== Done: $work/Sqliteman-$version.flatpak"
