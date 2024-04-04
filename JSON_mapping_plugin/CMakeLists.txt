# Plugins to build

# Dependencies
set ( CMAKE_CXX_STANDARD 17 ) 
set ( CMAKE_CXX_STANDARD_REQUIRED ON )
set ( CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std=c++17" )
set ( CMAKE_CXX17_EXTENSION_COMPILE_OPTION "-std=c++17" )
set ( CMAKE_CXX_EXTENSIONS ON )

set( CONFIGS
    config/json_mapping.cfg
)

set( JSON_LIBNAME JSON_mapping_plugin )

set(SOURCES
  JSON_mapping_plugin.cpp
  src/tmp.cpp
  src/handlers/mapping_handler.cpp
  src/map_types/base_entry.cpp
  src/map_types/map_entry.cpp
  src/map_types/dim_entry.cpp
  src/map_types/slice_entry.cpp
  src/map_types/expr_entry.cpp
  src/map_types/custom_entry.cpp
  src/utils/uda_plugin_helpers.cpp
  src/utils/scale_offset.cpp
)

set(HEADERS
  JSON_mapping_plugin.h
  src/tmp.hpp
  src/handlers/mapping_handler.hpp
  src/map_types/base_entry.hpp
  src/map_types/map_entry.hpp
  src/map_types/dim_entry.hpp
  src/map_types/slice_entry.hpp
  src/map_types/expr_entry.hpp
  src/map_types/custom_entry.hpp
  src/utils/uda_plugin_helpers.hpp
  src/utils/scale_offset.hpp
)

if(${PROJECT_NAME}_ENABLE_UNIT_TESTING)
    add_library(${PROJECT_NAME} ${SOURCES})
endif()

set(INCLUDE_DIRS
  include
  ext_include
  src
)

include( plugins )
uda_plugin(
    NAME IMAS_JSON_MAP
    ENTRY_FUNC jsonMappingPlugin
    DESCRIPTION "MAP IMAS element name to TOKAMAK data using JSON mapping files"
    EXAMPLE "IMAS_JSON_MAP::read()"
    LIBNAME ${JSON_LIBNAME}
    SOURCES ${SOURCES} 
    CONFIG_FILE ${CONFIGS}
    EXTRA_INCLUDE_DIRS
      ${INCLUDE_DIRS}
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
if(${PROJECT_NAME}_ENABLE_UNIT_TESTING)
    enable_testing()
    message(STATUS "Build unit tests for the project.")
    message(STATUS "Tests should always be found in the test folder")
    add_subdirectory(test)
endif()

#
# Format the project using the `clang-format` target
# (i.e: cmake --build build --target clang-format)
#
add_clang_format_target()
