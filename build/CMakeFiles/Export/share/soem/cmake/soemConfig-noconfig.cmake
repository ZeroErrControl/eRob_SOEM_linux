#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "soem" for configuration ""
set_property(TARGET soem APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(soem PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libsoem.so"
  IMPORTED_SONAME_NOCONFIG "libsoem.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS soem )
list(APPEND _IMPORT_CHECK_FILES_FOR_soem "${_IMPORT_PREFIX}/lib/libsoem.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
