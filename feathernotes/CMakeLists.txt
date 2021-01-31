set(QT_MINIMUM_VERSION "5.12.0")
set(HUNSPELL_MINIMUM_VERSION "1.6")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(Qt5Core "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Gui "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Widgets "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Svg "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Xml "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Network "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5PrintSupport "${QT_MINIMUM_VERSION}" REQUIRED)

# for spell checking (see FindHUNSPELL.cmake)
find_package(HUNSPELL "${HUNSPELL_MINIMUM_VERSION}" REQUIRED)

# optional localization
find_package(Qt5LinguistTools QUIET)

# X11 for Linux/Unix
if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  find_package(Qt5X11Extras "${QT_MINIMUM_VERSION}" REQUIRED)
  find_package(X11 REQUIRED)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# for the old code; not needed anymore
#find_package(MAGIC_INCLUDE_DIR magic.h)
#find_library(MAGIC_LIBRARY magic $ENV{MAGIC_LIB_PATH})

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt5Core_INCLUDE_DIRS}
                      ${Qt5Gui_INCLUDE_DIRS}
                      ${Qt5Widgets_INCLUDE_DIRS}
                      ${Qt5Svg_INCLUDE_DIRS}
                      ${Qt5Xml_INCLUDE_DIRS}
                      ${Qt5X11Extras_INCLUDE_DIRS}
                      ${Qt5Network_INCLUDE_DIRS}
                      ${Qt5PrintSupport_INCLUDE_DIRS}
                      ${X11_INCLUDE_DIR}
                      ${HUNSPELL_INCLUDE_DIRS})
  add_definitions(-DHAS_X11)
else()
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt5Core_INCLUDE_DIRS}
                      ${Qt5Gui_INCLUDE_DIRS}
                      ${Qt5Widgets_INCLUDE_DIRS}
                      ${Qt5Svg_INCLUDE_DIRS}
                      ${Qt5Xml_INCLUDE_DIRS}
                      ${Qt5Network_INCLUDE_DIRS}
                      ${Qt5PrintSupport_INCLUDE_DIRS}
                      ${HUNSPELL_INCLUDE_DIRS})
endif()

if(HAIKU)
  add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/data")
elseif(UNIX)
  add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")
endif()

set(feathernotes_SRCS
    colorLabel.cpp
    domitem.cpp
    dommodel.cpp
    find.cpp
    fn.cpp
    lineedit.cpp
    main.cpp
    pref.cpp
    simplecrypt.cpp
    spellChecker.cpp
    spellDialog.cpp
    svgicons.cpp
    textedit.cpp
    vscrollbar.cpp)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  set(feathernotes_SRCS ${feathernotes_SRCS} x11.cpp)
endif()

set(feathernotes_RESOURCES data/fn.qrc)
set(DESKTOP_FILE data/feathernotes.desktop)
set(ICON_FILE data/feathernotes.svg)

if(CMAKE_VERSION VERSION_LESS "3.10.0") # CMP0071 in 3.10: Let AUTOMOC and AUTOUIC process GENERATED files
  qt5_wrap_cpp(EXTRA_HEADERS messagebox.h filedialog.h help.h settings.h treeview.h)
else()
  set(EXTRA_HEADERS messagebox.h filedialog.h help.h settings.h treeview.h)
endif()

# translations
if(Qt5LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/feathernotes_*.ts)
  qt5_add_translation(QM_FILES ${TS_FILES})
  add_executable(feathernotes ${feathernotes_SRCS} ${feathernotes_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE} ${QM_FILES})
  if(HAIKU)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/translations")
  elseif(APPLE)
    SET_SOURCE_FILES_PROPERTIES(${QM_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/translations)
  elseif(UNIX)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/feathernotes/translations")
  endif()
else(Qt5LinguistTools_FOUND)
  add_executable(feathernotes ${feathernotes_SRCS} ${feathernotes_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE})
endif(Qt5LinguistTools_FOUND)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  target_link_libraries(feathernotes ${Qt5Core_LIBRARIES}
                                   ${Qt5Gui_LIBRARIES}
                                   ${Qt5Widgets_LIBRARIES}
                                   ${Qt5Svg_LIBRARIES}
                                   ${Qt5Xml_LIBRARIES}
                                   ${Qt5X11Extras_LIBRARIES}
                                   ${Qt5Network_LIBRARIES}
                                   ${Qt5PrintSupport_LIBRARIES}
                                   ${X11_LIBRARIES}
                                   ${HUNSPELL_LIBRARIES})
else()
  target_link_libraries(feathernotes ${Qt5Core_LIBRARIES}
                                   ${Qt5Gui_LIBRARIES}
                                   ${Qt5Widgets_LIBRARIES}
                                   ${Qt5Svg_LIBRARIES}
                                   ${Qt5Xml_LIBRARIES}
                                   ${Qt5Network_LIBRARIES}
                                   ${Qt5PrintSupport_LIBRARIES}
                                   ${HUNSPELL_LIBRARIES})
endif()

# installation
if(HAIKU)
  install(TARGETS feathernotes RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
elseif(APPLE)
  SET_TARGET_PROPERTIES(feathernotes PROPERTIES MACOSX_BUNDLE TRUE)
  INSTALL(TARGETS feathernotes DESTINATION ${CMAKE_INSTALL_PREFIX})
elseif(UNIX)
  # make symlink
  add_custom_target(fpad_symlink ALL ${CMAKE_COMMAND} -E create_symlink feathernotes fnotes DEPENDS feathernotes COMMENT "Creating fpad as a symlink to feathernotes")

  install(TARGETS feathernotes RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
  install(FILES ${DESKTOP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
  install(FILES ${ICON_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
  # install symlink
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fnotes DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()
