# Optional CommonMark renderer support.
find_package(PkgConfig)
pkg_check_modules(PKG_CMARK QUIET libcmark)

find_path(CMARK_INCLUDE_DIRS
          NAMES cmark.h
          HINTS ${PKG_CMARK_INCLUDE_DIRS})

find_library(CMARK_LIBRARIES
             NAMES ${PKG_CMARK_LIBRARIES} cmark libcmark
             HINTS ${PKG_CMARK_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CMARK
                                  REQUIRED_VARS CMARK_LIBRARIES CMARK_INCLUDE_DIRS
                                  VERSION_VAR PKG_CMARK_VERSION)

mark_as_advanced(CMARK_INCLUDE_DIRS CMARK_LIBRARIES)
