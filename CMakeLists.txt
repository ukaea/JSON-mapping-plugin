cmake_minimum_required( VERSION 3.15 )

#
# Project details
#
project(
    JSONMappingPlugin
    VERSION 0.1.0
    LANGUAGES CXX
)

#
# Extra cmake modules to find libraries
#
set( CMAKE_MODULE_PATH
     ${CMAKE_MODULE_PATH}
     ${CMAKE_SOURCE_DIR}/cmake
     ${CMAKE_SOURCE_DIR}/cmake/Modules
)

find_package( Boost REQUIRED )

#
# Set project options
#
include( StandardSettings )
include( StaticAnalyzers )
include( Utils )

add_clang_format_target()

message( STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}" )

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE "Debug" )
endif()
message( STATUS "Started CMake for ${PROJECT_NAME} v${PROJECT_VERSION}...\n" )

#
# Prevent building in the source directory
#
if( PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR )
  message( FATAL_ERROR "In-source builds not allowed.
    Please make a new directory (called a build directory)
    and run CMake from there.\n" )
endif()

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -pthread" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -pthread" )

#
# Create module
#
configure_file(
    ${CMAKE_SOURCE_DIR}/JSON_mapping_plugin/etc/module.in
    ${CMAKE_BINARY_DIR}/etc/JSON_mapping_plugin/${PROJECT_VERSION}
    @ONLY
)

set( MODULE_PATH "modulefiles/JSON_mapping_plugin" CACHE PATH "location to install module files to" )
set( MODULE_NAME "${PROJECT_VERSION}" CACHE STRING "name of the module file" )
string( CONFIGURE "${MODULE_PATH}" MODULE_PATH_EXPANDED @ONLY )

get_filename_component(
    MODULE_PATH_EXPANDED ${MODULE_PATH_EXPANDED}
    REALPATH BASE_DIR "${CMAKE_INSTALL_PREFIX}"
)

install(
    FILES ${CMAKE_BINARY_DIR}/etc/JSON_mapping_plugin/${PROJECT_VERSION}
    DESTINATION ${MODULE_PATH_EXPANDED}
    RENAME ${MODULE_NAME}
)

#
# Find UDA and RPC
#
if( TIRPC )
  find_package( TIRPC REQUIRED )
  include_directories( ${TIRPC_INCLUDE_DIRS} )
  link_libraries( ${TIRPC_LIBRARIES} )
  add_definitions( -DTIRPC )
endif()

find_package( UDA 2.7.0 REQUIRED )

set( UDA_HOME ${UDA_DIR} )
include_directories( ${UDA_CPP_INCLUDE_DIRS} )
include_directories( ${CMAKE_CURRENT_LIST_DIR} )

# This list is appended to by the uda_plugin macro
set( BUILT_PLUGINS "${PLUGINS}" )

# set( PLUGINS IMAS_JSON_MAP )
message( STATUS "PLUGINS: ${PLUGINS}" )
add_subdirectory( JSON_mapping_plugin )
# add_subdirectory( DRaFT_data_reader )
# add_subdirectory( temp_geom_plugin )

#
# Generate scripts
#
string( REGEX REPLACE ";" " " PLUGINS "${BUILT_PLUGINS}" )
configure_file(
    ${CMAKE_SOURCE_DIR}/scripts/activate-plugins.sh.in
    ${CMAKE_BINARY_DIR}/scripts/activate-plugins.sh
    @ONLY
)
configure_file(
    ${CMAKE_SOURCE_DIR}/scripts/remove-plugins.sh.in
    ${CMAKE_BINARY_DIR}/scripts/remove-plugins.sh
    @ONLY
)
