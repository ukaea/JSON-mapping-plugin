# - Try to find TIRPC
#
# To provide the module with a hint about where to find your TIRPC
# installation, you can set the environment variable TIRPC_ROOT. The Find
# module will then look in this path when searching for TIRPC paths and
# libraries.
#
# Once done this will define
#  TIRPC_FOUND - System has TIRPC
#  TIRPC_INCLUDE_DIRS - The TIRPC include directories
#  TIRPC_LIBRARIES - The libraries needed to use TIRPC

if( TIRPC_INCLUDE_DIRS AND TIRPC_LIBRARIES )
    # Already in cache, be silent
    set( TIRPC_FIND_QUIETLY TRUE )
endif( TIRPC_INCLUDE_DIRS AND TIRPC_LIBRARIES )

find_path( TIRPC_INCLUDE_DIR rpc/rpc.h
  HINTS ${TIRPC_ROOT}
    ENV TIRPC_ROOT
  PATHS
    /usr
    /usr/local
  PATH_SUFFIXES include include/tirpc
)

find_library( TIRPC_LIBRARY NAMES tirpc
  HINTS ${TIRPC_ROOT}
    ENV TIRPC_ROOT
  PATHS
    /usr
    /usr/local
  PATH_SUFFIXES lib lib64
)

set( TIRPC_LIBRARIES ${TIRPC_LIBRARY} )
set( TIRPC_INCLUDE_DIRS ${TIRPC_INCLUDE_DIR} )

include( FindPackageHandleStandardArgs )
find_package_handle_standard_args( TIRPC DEFAULT_MSG TIRPC_INCLUDE_DIR TIRPC_LIBRARY )

mark_as_advanced( TIRPC_INCLUDE_DIRS TIRPC_LIBRARIES )
