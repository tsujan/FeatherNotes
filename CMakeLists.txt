project(feathernotes)
cmake_minimum_required(VERSION 3.0)
set(CMAKE_BUILD_TYPE Release)
add_definitions(-Wall)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(HAIKU)
    set(CMAKE_INSTALL_PREFIX "/boot/home/config/non-packaged/apps/FeatherNotes")
  elseif(APPLE)
    set(CMAKE_INSTALL_PREFIX "/Applications")
  elseif(UNIX)
    set(CMAKE_INSTALL_PREFIX "/usr")
  endif()
endif()

option(WITH_HUNSPELL "Compiling with Hunspell..." ON)
option(WITHOUT_X11 "Compiling without X11..." OFF)

add_subdirectory(feathernotes)
