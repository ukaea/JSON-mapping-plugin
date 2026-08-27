# Project details
# project(
#     MappingPlugin
#     VERSION 0.2.0
#     LANGUAGES CXX
# )

find_package( UDA 2.7.0 REQUIRED )
find_package( LibTokaMap REQUIRED )

# Optional embedded-Python data-source support. When ON, the plugin can serve
# data sources implemented in Python (e.g. tokamap_compass CDBDataSource) by
# embedding CPython in uda_server. The interpreter starts lazily — only when
# the libtokamap config declares a [python_data_sources] table — so a plugin
# built with this ON still runs Python-free until such a source is configured.
# When OFF the plugin has no Python dependency at all.
option( MAPPING_PLUGIN_PYTHON "Embed CPython to serve Python data sources" OFF )

set( MAPPING_PLUGIN_PYTHON_SOURCES "" )
set( MAPPING_PLUGIN_PYTHON_INCLUDES "" )
set( MAPPING_PLUGIN_PYTHON_DEFINITIONS "" )
set( MAPPING_PLUGIN_PYTHON_LIBS "" )
if( MAPPING_PLUGIN_PYTHON )
  find_package( Python3 REQUIRED COMPONENTS Interpreter Development NumPy )
  # Only a single dlopen-able libpython path is wanted here. Python3_LIBRARIES
  # can be a multi-element list (and carries optimized;/debug; keywords on MSVC),
  # which would split -DMAPPING_PLUGIN_DEFAULT_LIBPYTHON="..." across two
  # compiler arguments and fail the compile on the unterminated quote.
  set( MAPPING_PLUGIN_LIBPYTHON "" )
  foreach( LIB IN LISTS Python3_LIBRARIES )
      if( LIB MATCHES "python" AND EXISTS "${LIB}" )
          set( MAPPING_PLUGIN_LIBPYTHON "${LIB}" )
          break()
      endif()
  endforeach()
  if( NOT MAPPING_PLUGIN_LIBPYTHON )
      message( WARNING
          "Could not pick a libpython path out of '${Python3_LIBRARIES}'; the plugin will need "
          "MAPPING_PLUGIN_LIBPYTHON set in the environment to start the embedded interpreter." )
  endif()
  set( MAPPING_PLUGIN_PYTHON_DEFINITIONS -DMAPPING_PLUGIN_PYTHON
      -DMAPPING_PLUGIN_DEFAULT_LIBPYTHON="${MAPPING_PLUGIN_LIBPYTHON}" )
  set( MAPPING_PLUGIN_PYTHON_INCLUDES ${Python3_INCLUDE_DIRS} ${Python3_NumPy_INCLUDE_DIRS} )
  set( MAPPING_PLUGIN_PYTHON_LIBS Python3::Python ${CMAKE_DL_LIBS} )
endif()
# Either way python_data_source.cpp is compiled: with MAPPING_PLUGIN_PYTHON it
# carries the real implementation, otherwise a stub that only fails loudly if
# the config actually requests Python sources.

include( plugins )
uda_plugin(
    NAME IMAS_MAP
    ENTRY_FUNC mappingPlugin
    DESCRIPTION "MAP IMAS element name to TOKAMAK data using JSON mapping files"
    EXAMPLE "IMAS_MAP::read()"
    LIBNAME mapping_plugin
    SOURCES mapping_plugin.cpp src/uda_plugin_helpers.cpp src/python_data_source.cpp
    CONFIG_FILE config/json_mapping.cfg config/libtokamap_config.toml
    EXTRA_INCLUDE_DIRS
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/ext_include
        ${UDA_CLIENT_INCLUDE_DIRS}
        ${MAPPING_PLUGIN_PYTHON_INCLUDES}
    EXTRA_DEFINITIONS
        ${MAPPING_PLUGIN_PYTHON_DEFINITIONS}
    EXTRA_LINK_DIRS
        ${UDA_CLIENT_LIBRARY_DIRS}
    EXTRA_LINK_LIBS
        LibTokaMap::libtokamap
        ${MAPPING_PLUGIN_PYTHON_LIBS}
)

# Generate scripts
list( GET UDA_DIR 0 UDA_HOME )
string( REGEX REPLACE ";" " " PLUGINS "${BUILT_PLUGINS}" )
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/activate-plugins.sh.in
    ${CMAKE_BINARY_DIR}/scripts/activate-plugins.sh
    @ONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/remove-plugins.sh.in
    ${CMAKE_BINARY_DIR}/scripts/remove-plugins.sh
    @ONLY
)
