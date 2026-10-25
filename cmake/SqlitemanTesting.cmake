# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

# Builds a Qt Test executable from <name>.cpp and registers it with CTest.
# Extra arguments are libraries to link against. A database is a file, so
# the tests make their own in a temporary directory and need no server.
function(sqliteman_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
    sqliteman_target_defaults(${name})
    # Before 6.10 (6.8 does, 6.10 does not), Qt Test's QTRY_ macros turn a
    # std::chrono timeout into an int inside the test's own code, which
    # -Wconversion reports, and -Werror fails on, in every test using them.
    if(Qt6_VERSION VERSION_LESS 6.10 AND NOT MSVC)
        target_compile_options(${name} PRIVATE -Wno-conversion)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Like sqliteman_add_test(), for tests of widgets: they run without a
# display, and with a settings directory of their own, so that neither the
# developer's preferences change what they see nor they change those.
function(sqliteman_add_gui_test name)
    sqliteman_add_test(${name} ${ARGN})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/test-config/${name}")
endfunction()
