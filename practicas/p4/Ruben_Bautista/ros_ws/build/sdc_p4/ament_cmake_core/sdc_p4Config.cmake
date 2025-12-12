# generated from ament/cmake/core/templates/nameConfig.cmake.in

# prevent multiple inclusion
if(_sdc_p4_CONFIG_INCLUDED)
  # ensure to keep the found flag the same
  if(NOT DEFINED sdc_p4_FOUND)
    # explicitly set it to FALSE, otherwise CMake will set it to TRUE
    set(sdc_p4_FOUND FALSE)
  elseif(NOT sdc_p4_FOUND)
    # use separate condition to avoid uninitialized variable warning
    set(sdc_p4_FOUND FALSE)
  endif()
  return()
endif()
set(_sdc_p4_CONFIG_INCLUDED TRUE)

# output package information
if(NOT sdc_p4_FIND_QUIETLY)
  message(STATUS "Found sdc_p4: 0.0.0 (${sdc_p4_DIR})")
endif()

# warn when using a deprecated package
if(NOT "" STREQUAL "")
  set(_msg "Package 'sdc_p4' is deprecated")
  # append custom deprecation text if available
  if(NOT "" STREQUAL "TRUE")
    set(_msg "${_msg} ()")
  endif()
  # optionally quiet the deprecation message
  if(NOT sdc_p4_DEPRECATED_QUIET)
    message(DEPRECATION "${_msg}")
  endif()
endif()

# flag package as ament-based to distinguish it after being find_package()-ed
set(sdc_p4_FOUND_AMENT_PACKAGE TRUE)

# include all config extra files
set(_extras "ament_cmake_export_dependencies-extras.cmake;ament_cmake_export_include_directories-extras.cmake;ament_cmake_export_libraries-extras.cmake")
foreach(_extra ${_extras})
  include("${sdc_p4_DIR}/${_extra}")
endforeach()
