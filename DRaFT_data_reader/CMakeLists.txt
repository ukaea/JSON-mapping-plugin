# Plugins to build

# Dependencies
set ( CMAKE_CXX_STANDARD 17 ) 
set ( CMAKE_CXX_STANDARD_REQUIRED ON )
set ( CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std=c++17" )
set ( CMAKE_CXX17_EXTENSION_COMPILE_OPTION "-std=c++17" )
set ( CMAKE_CXX_EXTENSIONS ON )

set( CONFIGS
    DRaFT_data_reader.cfg
)

include( plugins )
uda_plugin(
    NAME DRaFT_JSON
    ENTRY_FUNC DRaFTDataReader
    DESCRIPTION "Plugin to read DRaFT JSON data"
    EXAMPLE "DRaFT_JSON::get()"
    LIBNAME DRaFT_data_reader
    SOURCES DRaFT_data_reader.cpp
    CONFIG_FILE ${CONFIGS}
    EXTRA_INCLUDE_DIRS
      ${UDA_CLIENT_INCLUDE_DIRS}
    EXTRA_LINK_DIRS
      ${UDA_CLIENT_LIBRARY_DIRS}
    EXTRA_LINK_LIBS
      ${UDA_CLIENT_LIBRARIES}
      uda_cpp
)
