# With respect to the JSON_mapping_plugin subdirectory

set(SOURCES
    JSON_mapping_plugin.cpp
    src/tmp.cpp
    src/handlers/mapping_handler.cpp
    src/map_types/base_entry.cpp
)

#set(EXE_SOURCES
#    ${SOURCES}
#)

set(HEADERS
    JSON_mapping_plugin.h
    src/tmp.hpp
    src/handlers/mapping_handler.hpp
    src/map_types/base_entry.hpp
)

set(INCLUDE_DIRS
    include
    ext_include
    src
)

set(TEST_SOURCES
    src/tmp_test.cpp
)
