# - Find UDA
#
# Once done this will define
#
#  UDA_FOUND - System has UDA
#  UDA_INCLUDE_DIRS - The UDA include directory
#  UDA_LIBRARIES - The libraries needed to use UDA
#  UDA_DEFINITIONS - Compiler switches required for using UDA
#=======================================================================================================================

# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
find_package( PkgConfig )

pkg_check_modules( UDA_CLIENT uda-client )
pkg_check_modules( UDA_CPP uda-cpp )
pkg_check_modules( UDA_FAT_CLIENT uda-fat-client )
pkg_check_modules( UDA_FAT_CPP uda-fat-cpp )
pkg_check_modules( UDA_PLUGINS uda-plugins )

set( UDA_VERSION      ${UDA_CLIENT_VERSION} )
set( UDA_DEFINITIONS  ${UDA_CLIENT_CFLAGS_OTHER} )
set( UDA_INCLUDE_DIRS ${UDA_CLIENT_INCLUDE_DIRS} )
set( UDA_LIBRARY_DIRS ${UDA_CLIENT_LIBRARY_DIRS} )
set( UDA_LIBRARIES    ${UDA_CLIENT_LIBRARIES} )
set( UDA_DIR )
foreach( LIB_DIR ${UDA_LIBRARY_DIRS} )
  get_filename_component( DIR ${LIB_DIR} DIRECTORY )
  list( APPEND UDA_DIR ${DIR} )
endforeach()

# handle the QUIETLY and REQUIRED arguments and set UDA_FOUND to TRUE if
# all listed variables are TRUE
include( FindPackageHandleStandardArgs )
#find_package_handle_standard_args( UDA DEFAULT_MSG UDA_LIBRARIES UDA_INCLUDE_DIRS )
find_package_handle_standard_args( UDA
  FOUND_VAR UDA_FOUND
  REQUIRED_VARS
    UDA_LIBRARIES
    UDA_INCLUDE_DIRS
  VERSION_VAR UDA_VERSION
)

mark_as_advanced( UDA_INCLUDE_DIRS UDA_LIBRARIES )
