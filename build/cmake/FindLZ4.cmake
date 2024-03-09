#
# Find the LZ4 compression library
#
# export:
#
# LZ4_INCLUDE_DIR
# LZ4_LIBRARIES
# LZ4_FOUND
#

find_path(LZ4_INCLUDE_DIR
          NAMES lz4frame.h
          HINTS ${LZ4_ROOT_DIR}/include)

find_library(LZ4_LIBRARIES
             NAMES lz4
             HINTS ${LZ4_ROOT_DIR}/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LZ4 DEFAULT_MSG LZ4_LIBRARIES LZ4_INCLUDE_DIR)

mark_as_advanced(LZ4_LIBRARIES LZ4_INCLUDE_DIR)
