
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was rexglueConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

set(REXGLUE_VERSION_STRING "0.8.1.32-dev.gf22cd9d")

set(REXGLUE_SHARE_DIR "${PACKAGE_PREFIX_DIR}/share/rexglue")

include(CMakeFindDependencyMacro)

include("${CMAKE_CURRENT_LIST_DIR}/rexglue_helpers.cmake")

list(PREPEND CMAKE_PREFIX_PATH "${PACKAGE_PREFIX_DIR}")

find_dependency(fmt CONFIG)
find_dependency(spdlog CONFIG)
find_dependency(Snappy CONFIG)
find_dependency(utf8cpp CONFIG)
find_dependency(SDL3 CONFIG)

set(REXGLUE_USE_VULKAN OFF)
if(REXGLUE_USE_VULKAN)
    find_dependency(VulkanHeaders CONFIG)
    find_dependency(volk CONFIG)
    find_dependency(VulkanMemoryAllocator CONFIG)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/rexglueTargets.cmake")

if(TARGET rex::runtime AND NOT TARGET rex::system)
    add_library(rex::system ALIAS rex::runtime)
endif()
if(TARGET rex::runtime AND NOT TARGET rex::kernel)
    add_library(rex::kernel ALIAS rex::runtime)
endif()

if(NOT rexglue_FIND_QUIETLY)
    message(STATUS "Found ReXGlue SDK ${REXGLUE_VERSION_STRING} at (${PACKAGE_PREFIX_DIR})")
endif()

check_required_components(rexglue)
