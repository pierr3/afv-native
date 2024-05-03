set(AFV_NATIVE_INCLUDE_DIR ${afv_native_SOURCE_DIR}/include)

find_library(AFV_NATIVE_LIBRARY
  NAMES afv_native libafv_native
  PATHS ${afv_native_SOURCE_DIR}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(afv_native DEFAULT_MSG AFV_NATIVE_LIBRARY AFV_NATIVE_INCLUDE_DIR)

if(AFV_NATIVE_FOUND AND NOT TARGET afv_native)
  add_library(afv_native UNKNOWN IMPORTED)
  set_target_properties(afv_native PROPERTIES
    IMPORTED_LOCATION "${AFV_NATIVE_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${AFV_NATIVE_INCLUDE_DIR}"
  )
endif()