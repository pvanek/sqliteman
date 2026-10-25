# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

"""Sphinx configuration for the Sqliteman manual.

Two builders are used: ``html`` for the website and ``qthelp`` for the help
file the application itself shows. Keep the sources plain: whatever is
written here has to render in QTextBrowser as well as in a browser.
"""

import os
import re
from pathlib import Path

project = "Sqliteman"
author = "Petr Vanek"
copyright = "2007-2026, Petr Vanek and contributors"


def _version() -> str:
    """The version CMake is building, or the one in CMakeLists.txt."""
    if cmake_version := os.environ.get("SQLITEMAN_VERSION"):
        return cmake_version
    lists = Path(__file__).resolve().parent.parent / "CMakeLists.txt"
    found = re.search(r"VERSION\s+(\d+\.\d+\.\d+)", lists.read_text(encoding="utf-8"))
    return found.group(1) if found else "0.0.0"


version = release = _version()

extensions = ["myst_parser"]
myst_enable_extensions = ["colon_fence", "deflist"]
myst_heading_anchors = 3

exclude_patterns = ["_build"]
templates_path = ["_templates"]
language = "en"
nitpicky = True  # A link to a page that is not there fails the build.

# The website. The help file uses the "basic" theme, which is what
# QTextBrowser can render; see the qthelp options below.
html_theme = "furo"
html_title = f"{project} {version}"
# The program's own icon, taken from where the program keeps it. A PNG, not
# the SVG: the help viewer renders it with QTextBrowser.
html_logo = "../src/app/icons/sqliteman-128.png"
html_favicon = "../src/app/icons/sqliteman-32.png"
html_static_path = ["_static"] if (Path(__file__).parent / "_static").is_dir() else []
html_copy_source = False
html_show_sphinx = False

# The Qt help file: one .qch, registered by the application at startup.
qthelp_basename = "sqliteman"
qthelp_namespace = "cz.yarpen.sqliteman"
qthelp_theme = "basic"
