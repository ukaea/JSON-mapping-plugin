# With respect to the JSON_mapping_plugin subdirectory

set(SOURCES
    JSON_mapping_plugin.cpp
    src/tmp.cpp
    src/handlers/mapping_handler.cpp
    src/map_types/plugin_mapping.cpp
    src/map_types/dim_mapping.cpp
    src/map_types/slice_mapping.cpp
    src/map_types/expr_mapping.cpp
    src/map_types/custom_mapping.cpp
    src/map_types/value_mapping.cpp
    src/utils/uda_plugin_helpers.cpp
    src/utils/scale_offset.cpp
)

#set(EXE_SOURCES
#    ${SOURCES}
#)

set(HEADERS
    JSON_mapping_plugin.h
    src/tmp.hpp
    src/handlers/mapping_handler.hpp
    src/map_types/base_mapping.hpp
    src/map_types/plugin_mapping.hpp
    src/map_types/dim_mapping.hpp
    src/map_types/slice_mapping.hpp
    src/map_types/expr_mapping.hpp
    src/map_types/custom_mapping.hpp
    src/map_types/value_mapping.hpp
    src/utils/uda_plugin_helpers.hpp
    src/utils/scale_offset.hpp
)

set(INCLUDE_DIRS
    include
    ext_include
    src
)

set(TEST_SOURCES
    src/tmp_test.cpp
)
