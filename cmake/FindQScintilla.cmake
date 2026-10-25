# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

#[=======================================================================[.rst:
FindQScintilla
--------------

Finds the Qt 6 build of QScintilla (https://riverbankcomputing.com/software/qscintilla),
which installs no CMake package of its own. Looks next to Qt first.

Set ``QScintilla_ROOT`` to its install prefix if it is elsewhere.

Imported target: ``QScintilla::QScintilla``
Result variables: ``QScintilla_FOUND``, ``QScintilla_VERSION``
#]=======================================================================]

set(_qsci_hints "${QScintilla_ROOT}" ENV QScintilla_ROOT)
if(QT6_INSTALL_PREFIX)
    list(APPEND _qsci_hints "${QT6_INSTALL_PREFIX}")
endif()

find_path(QScintilla_INCLUDE_DIR
    NAMES Qsci/qsciscintilla.h
    HINTS ${_qsci_hints}
    PATH_SUFFIXES "${QT6_INSTALL_HEADERS}" include/qt6 include)

find_library(QScintilla_LIBRARY
    NAMES qscintilla2_qt6 qscintilla2_qt6d
    HINTS ${_qsci_hints}
    PATH_SUFFIXES "${QT6_INSTALL_LIBS}" lib lib64)

if(QScintilla_INCLUDE_DIR AND EXISTS "${QScintilla_INCLUDE_DIR}/Qsci/qsciglobal.h")
    file(STRINGS "${QScintilla_INCLUDE_DIR}/Qsci/qsciglobal.h" _qsci_version
        REGEX "#define QSCINTILLA_VERSION_STR")
    string(REGEX REPLACE ".*\"([0-9.]+)\".*" "\\1" QScintilla_VERSION "${_qsci_version}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QScintilla
    REQUIRED_VARS QScintilla_LIBRARY QScintilla_INCLUDE_DIR
    VERSION_VAR QScintilla_VERSION)

if(QScintilla_FOUND AND NOT TARGET QScintilla::QScintilla)
    add_library(QScintilla::QScintilla UNKNOWN IMPORTED)
    set_target_properties(QScintilla::QScintilla PROPERTIES
        IMPORTED_LOCATION "${QScintilla_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QScintilla_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES Qt6::Widgets)
    if(WIN32)
        set_property(TARGET QScintilla::QScintilla APPEND PROPERTY
            INTERFACE_COMPILE_DEFINITIONS QSCINTILLA_DLL)
    endif()
endif()

mark_as_advanced(QScintilla_INCLUDE_DIR QScintilla_LIBRARY)
unset(_qsci_hints)
unset(_qsci_version)
