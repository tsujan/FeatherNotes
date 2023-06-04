set(QT5_MINIMUM_VERSION "5.15.0")
set(QT6_MINIMUM_VERSION "6.3.0")
set(HUNSPELL_MINIMUM_VERSION "1.6")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

if(HAIKU)
  set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lnetwork")
elseif(OS2)
  # OS/2 needs an additional lib
  set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lcx")
endif()

if(NOT ENABLE_QT5)
  set(QT_VERSION_MAJOR 6)
  find_package(Qt6 "${QT6_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg Xml Network PrintSupport DBus)
else()
  set(QT_VERSION_MAJOR 5)
  find_package(Qt5 "${QT5_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg Xml Network PrintSupport DBus)
endif()

if(WITH_HUNSPELL)
# for spell checking (see FindHUNSPELL.cmake)
  find_package(HUNSPELL "${HUNSPELL_MINIMUM_VERSION}" REQUIRED)
endif()

# X11 for Linux/Unix
if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  if(QT_VERSION_MAJOR EQUAL 5)
    find_package(Qt5 "${QT5_MINIMUM_VERSION}" REQUIRED COMPONENTS X11Extras)
  endif()
  find_package(X11 REQUIRED)
endif()

# optional localization
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS LinguistTools)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(QT_VERSION_MAJOR EQUAL 6)
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt6Core_INCLUDE_DIRS}
                      ${Qt6Gui_INCLUDE_DIRS}
                      ${Qt6Widgets_INCLUDE_DIRS}
                      ${Qt6Svg_INCLUDE_DIRS}
                      ${Qt6Xml_INCLUDE_DIRS}
                      ${Qt6Network_INCLUDE_DIRS}
                      ${Qt6PrintSupport_INCLUDE_DIRS}
                      ${Qt6DBus_INCLUDE_DIRS})
else()
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt5Core_INCLUDE_DIRS}
                      ${Qt5Gui_INCLUDE_DIRS}
                      ${Qt5Widgets_INCLUDE_DIRS}
                      ${Qt5Svg_INCLUDE_DIRS}
                      ${Qt5Xml_INCLUDE_DIRS}
                      ${Qt5Network_INCLUDE_DIRS}
                      ${Qt5PrintSupport_INCLUDE_DIRS}
                      ${Qt5DBus_INCLUDE_DIRS})
endif()

if(WITH_HUNSPELL)
  include_directories(${include_directories} ${HUNSPELL_INCLUDE_DIRS})
  add_definitions(-DHAS_HUNSPELL)
endif()

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  if(QT_VERSION_MAJOR EQUAL 6)
    include_directories(${include_directories}
                        ${X11_INCLUDE_DIR})
  else()
    include_directories(${include_directories}
                        ${Qt5X11Extras_INCLUDE_DIRS}
                        ${X11_INCLUDE_DIR})
  endif()
  add_definitions(-DHAS_X11)
endif()

if(HAIKU)
  add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/data")
elseif(UNIX OR OS2)
  add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")
endif()

set(feathernotes_SRCS
    main.cpp
    singleton.cpp
    signalDaemon.cpp
    colorLabel.cpp
    fn.cpp
    find.cpp
    domitem.cpp
    dommodel.cpp
    lineedit.cpp
    pref.cpp
    textedit.cpp
    simplecrypt.cpp
    vscrollbar.cpp
    printing.cpp
    svgicons.cpp)

if(WITH_HUNSPELL)
  set(feathernotes_SRCS ${feathernotes_SRCS} spellChecker.cpp spellDialog.cpp)
endif()

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  set(feathernotes_SRCS ${feathernotes_SRCS} x11.cpp)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
  qt6_add_dbus_adaptor(feathernotes_DBUS_SRCS
      org.feathernotes.Application.xml
      singleton.h
      FeatherNotes::FNSingleton
      feathernotesadaptor
      FeatherNotesAdaptor
  )
else()
  qt5_add_dbus_adaptor(feathernotes_DBUS_SRCS
      org.feathernotes.Application.xml
      singleton.h
      FeatherNotes::FNSingleton
      feathernotesadaptor
      FeatherNotesAdaptor
  )
endif()

set(feathernotes_RESOURCES data/fn.qrc)
set(DESKTOP_FILE data/feathernotes.desktop)
set(METAINFO_FILE data/feathernotes.metainfo.xml)
set(XML_FILE data/feathernotes.xml)
if(NOT OS2)
  set(ICON_FILE data/feathernotes.svg)
  set(MIME_ICON_FILE data/text-feathernotes-fnx.svg)
else()
  enable_language(RC)
  set_source_files_properties(data/feathernotes_os2.rc PROPERTIES LANGUAGE RC)
  set(feathernotes_SRCS ${feathernotes_SRCS} data/feathernotes_os2.rc)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
  set(EXTRA_HEADERS warningbar.h spinbox.h messagebox.h filedialog.h help.h settings.h treeview.h treedelegate.h)
else()
  if(CMAKE_VERSION VERSION_LESS "3.10.0") # CMP0071 in 3.10: Let AUTOMOC and AUTOUIC process GENERATED files
    qt5_wrap_cpp(EXTRA_HEADERS warningbar.h spinbox.h messagebox.h filedialog.h help.h settings.h treeview.h treedelegate.h)
  else()
    set(EXTRA_HEADERS warningbar.h spinbox.h messagebox.h filedialog.h help.h settings.h treeview.h treedelegate.h)
  endif()
endif()

# translations
if(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/feathernotes_*.ts)
  if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_translation(QM_FILES ${TS_FILES})
  else()
    qt5_add_translation(QM_FILES ${TS_FILES})
  endif()
  add_executable(feathernotes ${feathernotes_SRCS} ${feathernotes_DBUS_SRCS} ${feathernotes_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE} ${QM_FILES})
  if(HAIKU)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/translations")
  elseif(APPLE)
    SET_SOURCE_FILES_PROPERTIES(${QM_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/translations)
  elseif(UNIX OR OS2)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/feathernotes/translations")
  endif()
else(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
  add_executable(feathernotes ${feathernotes_SRCS} ${feathernotes_DBUS_SRCS} ${feathernotes_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE})
endif(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)

if(QT_VERSION_MAJOR EQUAL 6)
  target_link_libraries(feathernotes ${Qt6Core_LIBRARIES}
                                     ${Qt6Gui_LIBRARIES}
                                     ${Qt6Widgets_LIBRARIES}
                                     ${Qt6Svg_LIBRARIES}
                                     ${Qt6Xml_LIBRARIES}
                                     ${Qt6Network_LIBRARIES}
                                     ${Qt6PrintSupport_LIBRARIES}
                                     ${Qt6DBus_LIBRARIES})
else()
  target_link_libraries(feathernotes ${Qt5Core_LIBRARIES}
                                     ${Qt5Gui_LIBRARIES}
                                     ${Qt5Widgets_LIBRARIES}
                                     ${Qt5Svg_LIBRARIES}
                                     ${Qt5Xml_LIBRARIES}
                                     ${Qt5Network_LIBRARIES}
                                     ${Qt5PrintSupport_LIBRARIES}
                                     ${Qt5DBus_LIBRARIES})
endif()

if(WITH_HUNSPELL)
  target_link_libraries(feathernotes ${HUNSPELL_LIBRARIES})
endif()

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(feathernotes ${X11_LIBRARIES})
  else()
    target_link_libraries(feathernotes ${Qt5X11Extras_LIBRARIES}
                                       ${X11_LIBRARIES})
  endif()
endif()

# installation
if(HAIKU)
  install(TARGETS feathernotes RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
elseif(APPLE)
  SET_TARGET_PROPERTIES(feathernotes PROPERTIES MACOSX_BUNDLE TRUE)
  INSTALL(TARGETS feathernotes DESTINATION ${CMAKE_INSTALL_PREFIX})
elseif(UNIX)
  install(TARGETS feathernotes RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
  install(FILES ${DESKTOP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
  install(FILES ${METAINFO_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/metainfo")
  install(FILES ${XML_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/mime/packages")
  install(FILES ${ICON_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
  install(FILES ${MIME_ICON_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/mimetypes")
elseif(OS2)
  SET_TARGET_PROPERTIES(feathernotes PROPERTIES OS2_DEF_EXEType "WINDOWAPI")
  install(TARGETS feathernotes RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()
