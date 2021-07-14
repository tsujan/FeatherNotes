set(QT_MINIMUM_VERSION "5.12.0")
set(HUNSPELL_MINIMUM_VERSION "1.6")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# OS/2 needs an additional lib and tune the processor
if(OS2)
  #add_definitions(-mtune=${CMAKE_SYSTEM_PROCESSOR} -march=${CMAKE_SYSTEM_PROCESSOR})
  set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lcx")
endif()

find_package(Qt5Core "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Gui "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Widgets "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Svg "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Xml "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Network "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5PrintSupport "${QT_MINIMUM_VERSION}" REQUIRED)

if(WITH_HUNSPELL)
# for spell checking (see FindHUNSPELL.cmake)
  find_package(HUNSPELL "${HUNSPELL_MINIMUM_VERSION}" REQUIRED)
endif()

# X11 for Linux/Unix
if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  find_package(Qt5X11Extras "${QT_MINIMUM_VERSION}" REQUIRED)
  find_package(X11 REQUIRED)
endif()

# optional localization
find_package(Qt5LinguistTools QUIET)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                    ${Qt5Core_INCLUDE_DIRS}
                    ${Qt5Gui_INCLUDE_DIRS}
                    ${Qt5Widgets_INCLUDE_DIRS}
                    ${Qt5Svg_INCLUDE_DIRS}
                    ${Qt5Xml_INCLUDE_DIRS}
                    ${Qt5Network_INCLUDE_DIRS}
                    ${Qt5PrintSupport_INCLUDE_DIRS})

if(WITH_HUNSPELL)
  include_directories(${include_directories} ${HUNSPELL_INCLUDE_DIRS})
  add_definitions(-DHAS_HUNSPELL)
endif()

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  include_directories(${include_directories}
                      ${Qt5X11Extras_INCLUDE_DIRS}
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
    svgicons.cpp)

if(WITH_HUNSPELL)
  set(feathernotes_SRCS ${feathernotes_SRCS} spellChecker.cpp spellDialog.cpp)
endif()

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  set(feathernotes_SRCS ${feathernotes_SRCS} x11.cpp)
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

target_link_libraries(feathernotes ${Qt5Core_LIBRARIES}
                                   ${Qt5Gui_LIBRARIES}
                                   ${Qt5Widgets_LIBRARIES}
                                   ${Qt5Svg_LIBRARIES}
                                   ${Qt5Xml_LIBRARIES}
                                   ${Qt5Network_LIBRARIES}
                                   ${Qt5PrintSupport_LIBRARIES})

if(WITH_HUNSPELL)
  target_link_libraries(feathernotes ${HUNSPELL_LIBRARIES})
endif()

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  target_link_libraries(feathernotes ${Qt5X11Extras_LIBRARIES}
                                     ${X11_LIBRARIES})
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
  SET_TARGET_PROPERTIES(feathernotes PROPERTIES
  	OS2_DEF_EXEType "WINDOWAPI"
  )
  install(TARGETS feathernotes RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
  install(FILES ${HELP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/feathernotes")
  # also install translated help files
  install(FILES ${HELP_TR_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/feathernotes")
endif()
