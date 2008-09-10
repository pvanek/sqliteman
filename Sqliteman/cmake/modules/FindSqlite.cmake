# - Try to find the SQLITE library
# Once done this will define
#
#  SQLITE_FOUND - system has sqlite
#  SQLITE_INCLUDE_DIR - the sqlite include directory
#  SQLITE_LIBRARIES - Link these to use sqlite
#  SQLITE_DEFINITIONS - Compiler switches required for using sqlite
#

if (SQLITE_INCLUDE_DIR AND SQLITE_LIBRARIES)

  # in cache already
  SET(SQLITE_FOUND TRUE)

else (SQLITE_INCLUDE_DIR AND SQLITE_LIBRARIES)
  IF (NOT WIN32)
    # use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    INCLUDE(UsePkgConfig)
  
    PKGCONFIG(sqlite3 _SQLITEIncDir _SQLITELinkDir _SQLITELinkFlags _SQLITECflags)
  
    set(SQLITE_DEFINITIONS ${_SQLITECflags})
  ENDIF (NOT WIN32)

  FIND_PATH(SQLITE_INCLUDE_DIR sqlite3.h
    ${_SQLITEIncDir}
  )
  
  FIND_LIBRARY(SQLITE_LIBRARIES NAMES sqlite3
    PATHS
    ${_SQLITELinkDir}
  )
 
  
  include(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(Sqlite DEFAULT_MSG SQLITE_INCLUDE_DIR SQLITE_LIBRARIES )
  
  MARK_AS_ADVANCED(SQLITE_INCLUDE_DIR SQLITE_LIBRARIES)
  
endif (SQLITE_INCLUDE_DIR AND SQLITE_LIBRARIES)
