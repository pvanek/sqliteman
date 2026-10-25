# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

include(GNUInstallDirs)

find_program(SQLITEMAN_SPHINX_BUILD NAMES sphinx-build sphinx-build-3)
if(NOT SQLITEMAN_SPHINX_BUILD)
    message(FATAL_ERROR
        "SQLITEMAN_WITH_DOCS is on but sphinx-build was not found. "
        "Install Sphinx with the MyST parser, e.g. on openSUSE: "
        "zypper install python313-Sphinx python313-myst-parser python313-furo")
endif()

set(docs_source "${CMAKE_CURRENT_SOURCE_DIR}")
set(docs_binary "${CMAKE_CURRENT_BINARY_DIR}")

# The manual shows the program's icon, and Sphinx takes images only from its
# own directory. The copy is not in the repository: it is made here, so a
# fresh clone needs nothing but the SVG's rendered PNGs.
set(docs_logo "${docs_source}/images/sqliteman.png")
add_custom_command(
    OUTPUT "${docs_logo}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/app/icons/sqliteman-128.png" "${docs_logo}"
    DEPENDS "${CMAKE_SOURCE_DIR}/src/app/icons/sqliteman-128.png"
    COMMENT "Copying the icon into the manual"
    VERBATIM)
add_custom_target(docs-logo DEPENDS "${docs_logo}")

# The website. Sphinx keeps its own cache, so a second run only rebuilds
# the pages that changed.
add_custom_target(docs-html
    COMMAND ${CMAKE_COMMAND} -E env "SQLITEMAN_VERSION=${PROJECT_VERSION}"
        "${SQLITEMAN_SPHINX_BUILD}" -b html
        -d "${docs_binary}/cache/html" "${docs_source}" "${docs_binary}/html"
    WORKING_DIRECTORY "${docs_source}"
    COMMENT "Building the manual as a website"
    VERBATIM)
add_dependencies(docs-html docs-logo)

# The help file the program shows. Sphinx writes plain HTML and a .qhp
# project; qhelpgenerator turns those into one .qch, full-text index and all.
find_program(SQLITEMAN_QHELPGENERATOR
    NAMES qhelpgenerator qhelpgenerator-qt6
    HINTS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_LIBEXECS}" "${QT6_INSTALL_PREFIX}/libexec"
        /usr/libexec/qt6 /usr/lib64/qt6/libexec
    PATH_SUFFIXES libexec)

if(SQLITEMAN_QHELPGENERATOR)
    set(qthelp_dir "${docs_binary}/qthelp")
    # Beside the program, so a build directory run finds it too; in a macOS
    # bundle that is Resources, as anything in Contents/MacOS is taken for
    # code by macdeployqt and codesign.
    if(APPLE)
        set(qch_beside "$<TARGET_BUNDLE_CONTENT_DIR:sqliteman>/Resources")
    else()
        set(qch_beside "$<TARGET_FILE_DIR:sqliteman>")
    endif()
    # A plain target, run whenever it is asked for: Sphinx is incremental
    # itself, and listing every page as a dependency here would go stale the
    # moment a page is added.
    add_custom_target(docs-qch
        COMMAND ${CMAKE_COMMAND} -E env "SQLITEMAN_VERSION=${PROJECT_VERSION}"
            "${SQLITEMAN_SPHINX_BUILD}" -b qthelp -q
            -d "${docs_binary}/cache/qthelp" "${docs_source}" "${qthelp_dir}"
        COMMAND "${SQLITEMAN_QHELPGENERATOR}" "${qthelp_dir}/sqliteman.qhp"
            -o "${docs_binary}/sqliteman.qch"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${qch_beside}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${docs_binary}/sqliteman.qch"
            "${qch_beside}/sqliteman.qch"
        WORKING_DIRECTORY "${docs_source}"
        COMMENT "Building the manual as a Qt help file"
        VERBATIM)
    add_dependencies(docs-qch docs-logo)

    # A macOS bundle keeps its data inside itself.
    if(APPLE)
        set(qch_destination sqliteman.app/Contents/Resources)
    else()
        set(qch_destination "${CMAKE_INSTALL_DATADIR}/sqliteman")
    endif()
    install(FILES "${docs_binary}/sqliteman.qch"
        DESTINATION "${qch_destination}"
        OPTIONAL)
else()
    message(STATUS "qhelpgenerator was not found: no help file will be built")
endif()

add_custom_target(docs DEPENDS docs-html)
if(SQLITEMAN_QHELPGENERATOR)
    add_dependencies(docs docs-qch)
endif()
