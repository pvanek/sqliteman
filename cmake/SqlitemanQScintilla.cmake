# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

# Provides the QScintilla::QScintilla target: the system's Qt 6 build of
# QScintilla when there is one, otherwise a copy downloaded and built as part
# of this project (the usual case on Windows and macOS).

option(SQLITEMAN_BUNDLED_QSCINTILLA
    "Always download and build QScintilla instead of using a system copy" OFF)

if(NOT SQLITEMAN_BUNDLED_QSCINTILLA)
    find_package(QScintilla 2.13)
    if(QScintilla_FOUND)
        return()
    endif()
    message(STATUS "QScintilla for Qt 6 not found; building a bundled copy")
endif()

# The URL hash pins the exact tarball; update both together.
include(FetchContent)
FetchContent_Declare(qscintilla
    URL https://www.riverbankcomputing.com/static/Downloads/QScintilla/2.14.1/QScintilla_src-2.14.1.tar.gz
    URL_HASH SHA256=dfe13c6acc9d85dfcba76ccc8061e71a223957a6c02f3c343b30a9d43a4cdd4d
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(qscintilla)
message(STATUS "Using bundled QScintilla 2.14.1")

# Mirrors src/qscintilla.pro, without the printer (and so Qt PrintSupport).
set(_qsci "${qscintilla_SOURCE_DIR}")
file(GLOB _qsci_sources CONFIGURE_DEPENDS
    "${_qsci}/src/*.cpp"
    "${_qsci}/scintilla/src/*.cpp"
    "${_qsci}/scintilla/lexlib/*.cpp"
    "${_qsci}/scintilla/lexers/*.cpp")
# Headers with Q_OBJECT, so AUTOMOC sees them.
file(GLOB _qsci_headers CONFIGURE_DEPENDS "${_qsci}/src/*.h" "${_qsci}/src/Qsci/*.h")
list(FILTER _qsci_sources EXCLUDE REGEX "qsciprinter\\.cpp$")
list(FILTER _qsci_headers EXCLUDE REGEX "qsciprinter\\.h$")

add_library(sqliteman_qscintilla STATIC ${_qsci_sources} ${_qsci_headers})
add_library(QScintilla::QScintilla ALIAS sqliteman_qscintilla)
# SYSTEM, as an installed QScintilla's headers are: our warnings, and
# -Werror, are for our code, not for what its headers do.
target_include_directories(sqliteman_qscintilla SYSTEM
    PUBLIC "${_qsci}/src"
    PRIVATE "${_qsci}/scintilla/include" "${_qsci}/scintilla/lexlib" "${_qsci}/scintilla/src")
target_compile_definitions(sqliteman_qscintilla PRIVATE
    SCINTILLA_QT SCI_LEXER INCLUDE_DEPRECATED_FEATURES)
target_link_libraries(sqliteman_qscintilla PUBLIC Qt6::Widgets)
set_target_properties(sqliteman_qscintilla PROPERTIES AUTOMOC ON AUTOUIC OFF AUTORCC OFF)
# Third-party code: keep its warnings out of our build log.
if(MSVC)
    target_compile_options(sqliteman_qscintilla PRIVATE /w)
else()
    target_compile_options(sqliteman_qscintilla PRIVATE -w)
endif()

unset(_qsci)
unset(_qsci_sources)
unset(_qsci_headers)
