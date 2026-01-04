set(QT6_MINIMUM_VERSION "6.5.0")
set(HUNSPELL_MINIMUM_VERSION "1.6")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

if(HAIKU)
  set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lnetwork")
elseif(OS2)
  # OS/2 needs an additional lib
  set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lcx")
endif()

find_package(Qt6 "${QT6_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg Xml Network PrintSupport DBus)

if(WITH_HUNSPELL)
# for spell checking (see FindHUNSPELL.cmake)
  find_package(HUNSPELL "${HUNSPELL_MINIMUM_VERSION}" REQUIRED)
endif()

# X11 for Linux/Unix
if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  find_package(X11 REQUIRED)
endif()

# optional localization
find_package(Qt6 QUIET COMPONENTS LinguistTools)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                    ${Qt6Core_INCLUDE_DIRS}
                    ${Qt6Gui_INCLUDE_DIRS}
                    ${Qt6Widgets_INCLUDE_DIRS}
                    ${Qt6Svg_INCLUDE_DIRS}
                    ${Qt6Xml_INCLUDE_DIRS}
                    ${Qt6Network_INCLUDE_DIRS}
                    ${Qt6PrintSupport_INCLUDE_DIRS}
                    ${Qt6DBus_INCLUDE_DIRS})

if(WITH_HUNSPELL)
  include_directories(${include_directories} ${HUNSPELL_INCLUDE_DIRS})
  add_definitions(-DHAS_HUNSPELL)
endif()

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  include_directories(${include_directories}
                      ${X11_INCLUDE_DIR})
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

qt6_add_dbus_adaptor(feathernotes_DBUS_SRCS
    org.feathernotes.Application.xml
    singleton.h
    FeatherNotes::FNSingleton
    feathernotesadaptor
    FeatherNotesAdaptor
)

set(feathernotes_RESOURCES data/fn.qrc)
set(DESKTOP_FILE data/feathernotes.desktop)
set(METAINFO_FILE data/feathernotes.metainfo.xml)
set(XML_FILE data/feathernotes.xml)
if(APPLE)
  set(MACOSX_BUNDLE_ICON_FILE feathernotes)
  set(APP_ICON_MACOS data/feathernotes.icns)
  set_source_files_properties(${APP_ICON_MACOS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()
if(NOT OS2)
  set(ICON_FILE data/feathernotes.svg)
  set(MIME_ICON_FILE data/text-feathernotes-fnx.svg)
else()
  enable_language(RC)
  set_source_files_properties(data/feathernotes_os2.rc PROPERTIES LANGUAGE RC)
  set(feathernotes_SRCS ${feathernotes_SRCS} data/feathernotes_os2.rc)
endif()

set(EXTRA_HEADERS warningbar.h spinbox.h messagebox.h filedialog.h help.h settings.h treeview.h treedelegate.h)

# translations
if(Qt6LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/feathernotes_*.ts)
  qt6_add_translation(QM_FILES ${TS_FILES})
  add_executable(feathernotes ${feathernotes_SRCS} ${feathernotes_DBUS_SRCS} ${feathernotes_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE} ${QM_FILES})
  if(HAIKU)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/translations")
  elseif(APPLE)
    SET_SOURCE_FILES_PROPERTIES(${QM_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/translations)
  elseif(UNIX OR OS2)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/feathernotes/translations")
  endif()
else(Qt6LinguistTools_FOUND)
  add_executable(feathernotes ${feathernotes_SRCS} ${feathernotes_DBUS_SRCS} ${feathernotes_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE})
endif(Qt6LinguistTools_FOUND)

target_link_libraries(feathernotes ${Qt6Core_LIBRARIES}
                                   ${Qt6Gui_LIBRARIES}
                                   ${Qt6Widgets_LIBRARIES}
                                   ${Qt6Svg_LIBRARIES}
                                   ${Qt6Xml_LIBRARIES}
                                   ${Qt6Network_LIBRARIES}
                                   ${Qt6PrintSupport_LIBRARIES}
                                   ${Qt6DBus_LIBRARIES})

if(WITH_HUNSPELL)
  target_link_libraries(feathernotes ${HUNSPELL_LIBRARIES})
endif()

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  target_link_libraries(feathernotes ${X11_LIBRARIES})
endif()

# installation
if(HAIKU)
  install(TARGETS feathernotes RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
elseif(APPLE)
  target_sources(feathernotes PRIVATE ${APP_ICON_MACOS})
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
