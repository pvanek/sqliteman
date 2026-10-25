#!/bin/sh
# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Renders the application icon from sqliteman.svg into the PNGs, the Windows
# .ico kept next to it, and the manual's copy in docs/images. Needs Inkscape
# and ImageMagick; run it after changing the SVG and commit the results.
set -e
cd "$(dirname "$0")"
for size in 16 22 24 32 48 64 128 256 512; do
    inkscape sqliteman.svg --export-type=png --export-width=$size --export-height=$size \
        --export-filename=sqliteman-$size.png
done
magick sqliteman-16.png sqliteman-24.png sqliteman-32.png sqliteman-48.png sqliteman-64.png \
    sqliteman-256.png sqliteman.ico

# The manual shows the same icon, and Sphinx only takes images that live in
# its own directory.
mkdir -p ../../../docs/images
cp sqliteman-128.png ../../../docs/images/sqliteman.png
