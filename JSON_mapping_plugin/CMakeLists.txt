# Plugins to build

# Dependencies
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std=c++17" )
set( CMAKE_CXX17_EXTENSION_COMPILE_OPTION "-std=c++17" )
set( CMAKE_CXX_EXTENSIONS ON )

set( CONFIGS
     config/json_mapping.cfg
)

set( JSON_LIBNAME JSON_mapping_plugin )

set( SOURCES
    JSON_mapping_plugin.cpp
    src/utils/scale_offset.cpp
    src/utils/uda_plugin_helpers.cpp
    src/utils/ram_cache.cpp
    src/utils/subset.cpp
    src/map_types/custom_mapping.cpp
    src/map_types/dim_mapping.cpp
    src/map_types/expr_mapping.cpp
    src/map_types/plugin_mapping.cpp
    src/map_types/value_mapping.cpp
    src/handlers/mapping_handler.cpp
)

set( HEADERS
    JSON_mapping_plugin.h
    src/utils/scale_offset.hpp
    src/utils/uda_plugin_helpers.hpp
    src/utils/ram_cache.hpp
    src/utils/subset.hpp
    src/map_types/base_mapping.hpp
    src/map_types/custom_mapping.hpp
    src/map_types/dim_mapping.hpp
    src/map_types/expr_mapping.hpp
    src/map_types/map_arguments.hpp
    src/map_types/plugin_mapping.hpp
    src/map_types/value_mapping.hpp
    src/handlers/mapping_handler.hpp
)

include( plugins )
uda_plugin(
    NAME IMAS_JSON_MAP
    ENTRY_FUNC jsonMappingPlugin
    DESCRIPTION "MAP IMAS element name to TOKAMAK data using JSON mapping files"
    EXAMPLE "IMAS_JSON_MAP::read()"
    LIBNAME ${JSON_LIBNAME}
    SOURCES ${SOURCES} ${HEADERS}
    CONFIG_FILE ${CONFIGS}
    EXTRA_INCLUDE_DIRS
      ${CMAKE_CURRENT_LIST_DIR}/src
      ${CMAKE_CURRENT_LIST_DIR}/ext_include
      ${UDA_CLIENT_INCLUDE_DIRS}
      ${Boost_INCLUDE_DIRS}
    EXTRA_LINK_DIRS
      ${UDA_CLIENT_LIBRARY_DIRS}
      ${Boost_LIBRARY_DIRS}
    EXTRA_LINK_LIBS
      ${UDA_CLIENT_LIBRARIES}
      ${Boost_LIBRARIES}
      uda_cpp
)

#
# Unit testing setup
#
if( ${PROJECT_NAME}_ENABLE_UNIT_TESTING )
  enable_testing()
  message( STATUS "Build unit tests for the project." )
  message( STATUS "Tests should always be found in the test folder" )
  add_subdirectory( test )
endif()

list( TRANSFORM SOURCES PREPEND ${CMAKE_CURRENT_LIST_DIR}/ )
list( TRANSFORM HEADERS PREPEND ${CMAKE_CURRENT_LIST_DIR}/ )

set_property( TARGET clang-format APPEND PROPERTY SOURCES ${SOURCES} ${HEADERS} )
