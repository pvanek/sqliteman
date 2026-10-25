# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copies the DLLs a program needs, other than Windows' own, next to it.
# windeployqt does that for Qt only. SQLite and QScintilla are linked in
# statically, so this mostly finds nothing to do; it is here so that a DLL
# nobody expected fails the build, instead of the program on a user's machine.
#
#   cmake -DEXECUTABLE=dir/prog.exe -DSEARCH_DIRS="a;b" -P deploy-dlls.cmake

cmake_minimum_required(VERSION 3.25)

if(NOT EXECUTABLE OR NOT SEARCH_DIRS)
    message(FATAL_ERROR "Pass -DEXECUTABLE=<program> and -DSEARCH_DIRS=<dir;dir>")
endif()
cmake_path(GET EXECUTABLE PARENT_PATH target_dir)

# The plugins windeployqt put there can load libraries too.
file(GLOB_RECURSE plugins "${target_dir}/*.dll")

# Only Windows itself besides SEARCH_DIRS. CMake also looks in every PATH
# directory, and on a build machine those hold the DLLs of all its tools:
# one of those would be bundled, and its dependencies looked for too.
set(ENV{PATH} "$ENV{SystemRoot}\\System32;$ENV{SystemRoot}")

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${EXECUTABLE}"
    LIBRARIES ${plugins}
    DIRECTORIES "${target_dir}" ${SEARCH_DIRS}
    PRE_EXCLUDE_REGEXES "^[Aa][Pp][Ii]-[Mm][Ss]-" "^[Ee][Xx][Tt]-[Mm][Ss]-"
    # Windows' own: anything in its directory, however the path is spelled.
    POST_EXCLUDE_REGEXES "^[A-Za-z]:[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\]"
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
    CONFLICTING_DEPENDENCIES_PREFIX conflicting)

if(unresolved)
    message(FATAL_ERROR "Not found in ${SEARCH_DIRS}: ${unresolved}")
endif()
foreach(name IN LISTS conflicting_FILENAMES)
    message(WARNING "${name} found in more than one place: ${conflicting_${name}}; "
        "using the first")
    list(GET conflicting_${name} 0 first)
    list(APPEND resolved "${first}")
endforeach()

foreach(dll IN LISTS resolved)
    cmake_path(GET dll PARENT_PATH dir)
    cmake_path(COMPARE "${dir}" EQUAL "${target_dir}" already_there)
    if(NOT already_there)
        message(STATUS "Bundling ${dll}")
        file(COPY "${dll}" DESTINATION "${target_dir}")
    endif()
endforeach()
