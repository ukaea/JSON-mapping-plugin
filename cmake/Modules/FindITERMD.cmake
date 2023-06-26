# - Try to find ITERMD
# Once done this will define
#
#  ITERMD_FOUND - system has ITERMD
#  ITERMD_INCLUDE_DIRS - the ITERMD include directory
#  ITERMD_LIBRARIES - Link these to use ITERMD
#  ITERMD_DEFINITIONS - Compiler switches required for using ITERMD
#

if( ITERMD_LIBRARIES AND ITERMD_INCLUDE_DIRS )
  # Already in cache, be silent
  set( ITERMD_FIND_QUIETLY TRUE )
endif( ITERMD_LIBRARIES AND ITERMD_INCLUDE_DIRS )

find_path( ITERMD_INCLUDE_DIRS IDSDBHelper.h
  HINTS ${ITERMD_ROOT}
    ENV ITERMD_ROOT
  PATHS
    /usr/local
    /opt/local
    /sw
    /usr/lib/sfw
  PATH_SUFFIXES include )

find_library( ITERMD_LIBRARIES NAMES machinedescription
  HINTS ${ITERMD_ROOT}
    ENV ITERMD_ROOT
  PATHS
    /opt/local
    /sw
    /usr
    /usr/local
  PATH_SUFFIXES lib lib64 )

include( FindPackageHandleStandardArgs )
find_package_handle_standard_args( ITERMD DEFAULT_MSG ITERMD_LIBRARIES ITERMD_INCLUDE_DIRS )

# show the ITERMD_INCLUDE_DIRS and ITERMD_LIBRARIES variables only in the advanced view
mark_as_advanced( ITERMD_INCLUDE_DIRS ITERMD_LIBRARIES )
