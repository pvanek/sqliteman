INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/sqliteman/doc/en/
        DESTINATION ${DOC_DIR}/en/
        USE_SOURCE_PERMISSIONS
          PATTERN ".svn" EXCLUDE
          PATTERN "CMakeFiles" EXCLUDE
          PATTERN "Makefile" EXCLUDE
          PATTERN "cmake_install.cmake" EXCLUDE
          PATTERN "CMakeCache.txt" EXCLUDE
          PATTERN "CMakeLists.txt" EXCLUDE
          PATTERN "install_manifest.txt" EXCLUDE
          PATTERN "*.html"
          PATTERN "*.css"
          PATTERN "*.png"
)
