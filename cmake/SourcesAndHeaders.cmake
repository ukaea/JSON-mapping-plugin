# With respect to the JSON_mapping_plugin subdirectory

set(SOURCES
    JSON_mapping_plugin.cpp
    src/tmp.cpp
)

#set(EXE_SOURCES
#    ${SOURCES}
#)

# set(HEADERS
#     JSON_mapping_plugin.h
#     include/tmp.hpp
# )

set(INCLUDE_DIRS
    include
    ext_include
    src
)

set(TEST_SOURCES
    src/tmp_test.cpp
)
