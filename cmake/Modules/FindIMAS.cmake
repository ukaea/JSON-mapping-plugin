# - Find IMAS
#
# Once done this will define
#
#  IMAS_FOUND - System has IMAS
#  IMAS_INCLUDE_DIRS - The IMAS include directory
#  IMAS_LIBRARIES - The libraries needed to use IMAS
#  IMAS_DEFINITIONS - Compiler switches required for using IMAS
#=======================================================================================================================

# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
find_package( PkgConfig )

pkg_check_modules( IMAS_LOW_LEVEL imas-lowlevel )
pkg_check_modules( IMAS_CPP imas-cpp )

set( IMAS_DEFINITIONS  ${IMAS_LOW_LEVEL_CFLAGS_OTHER} )
set( IMAS_INCLUDE_DIRS ${IMAS_LOW_LEVEL_INCLUDE_DIRS} )
set( IMAS_LIBRARY_DIRS ${IMAS_LOW_LEVEL_LIBRARY_DIRS} )
set( IMAS_LIBRARIES    ${IMAS_LOW_LEVEL_LIBRARIES} )

# handle the QUIETLY and REQUIRED arguments and set IMAS_FOUND to TRUE if
# all listed variables are TRUE
include( FindPackageHandleStandardArgs )
find_package_handle_standard_args( IMAS DEFAULT_MSG IMAS_LIBRARIES IMAS_INCLUDE_DIRS )

mark_as_advanced( IMAS_INCLUDE_DIRS IMAS_LIBRARIES )
