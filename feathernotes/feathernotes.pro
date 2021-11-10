lessThan(QT_MAJOR_VERSION, 5) {
  error("FeatherNotes needs Qt5")
}
else {
  lessThan(QT_MAJOR_VERSION, 6) {
    lessThan(QT_MINOR_VERSION, 12) {
      error("FeatherNotes needs at least Qt 5.12.0")
    }
  }
}

QT += core gui \
      xml \
      widgets \
      printsupport \
      svg

haiku|macx {
  TARGET = FeatherNotes
}
else {
  TARGET = feathernotes
}

TEMPLATE = app
CONFIG += c++11

SOURCES += main.cpp \
           colorLabel.cpp \
           fn.cpp \
           find.cpp \
           domitem.cpp \
           dommodel.cpp \
           lineedit.cpp \
           pref.cpp \
           textedit.cpp \
           simplecrypt.cpp \
           vscrollbar.cpp \
           svgicons.cpp

HEADERS += fn.h \
           colorLabel.h \
           domitem.h \
           dommodel.h \
           textedit.h \
           lineedit.h \
           pref.h \
           spinbox.h \
           simplecrypt.h \
           vscrollbar.h \
           settings.h \
           help.h \
           filedialog.h \
           treeview.h \
           messagebox.h \
           svgicons.h

FORMS += fn.ui \
         prefDialog.ui \
         helpDialog.ui \
         about.ui

RESOURCES += data/fn.qrc

contains(WITHOUT_X11, YES) {
  message("Compiling without X11...")
}
else:unix:!macx:!haiku:!os2 {
  QT += x11extras
  SOURCES += x11.cpp
  HEADERS += x11.h
  LIBS += -lX11
  DEFINES += HAS_X11
}

# OS/2's qmake port doesn't seem to handle setting VARIABLES
os2 {
  WITH_HUNSPELL=YES
}

contains(WITH_HUNSPELL, YES) {
  CONFIG += link_pkgconfig
  PKGCONFIG += hunspell
  SOURCES += spellDialog.cpp spellChecker.cpp
  HEADERS += spellDialog.h spellChecker.h
  FORMS += spellDialog.ui
  DEFINES += HAS_HUNSPELL
}

unix {
  #TRANSLATIONS
  exists($$[QT_INSTALL_BINS]/lrelease) {
    TRANSLATIONS = $$system("find data/translations/ -name 'feathernotes_*.ts'")
    updateqm.input = TRANSLATIONS
    updateqm.output = data/translations/translations/${QMAKE_FILE_BASE}.qm
    updateqm.commands = $$[QT_INSTALL_BINS]/lrelease ${QMAKE_FILE_IN} -qm data/translations/translations/${QMAKE_FILE_BASE}.qm
    updateqm.CONFIG += no_link target_predeps
    QMAKE_EXTRA_COMPILERS += updateqm
  }
}
else:win32|os2 {
  #TRANSLATIONS
  exists($$[QT_INSTALL_BINS]/lrelease.exe) {
  	win32 {
      TRANSLATIONS = $$system("dir /b /S feathernotes_*.ts")
    }
    os2 {
      TRANSLATIONS = $$system("find data/translations/ -name 'feathernotes_*.ts'")
    }
    updateqm.input = TRANSLATIONS
    updateqm.output = data\\translations\\translations\\${QMAKE_FILE_BASE}.qm
    updateqm.commands = $$[QT_INSTALL_BINS]/lrelease ${QMAKE_FILE_IN} -qm data\\translations\\translations\\${QMAKE_FILE_BASE}.qm
    updateqm.CONFIG += no_link target_predeps
    QMAKE_EXTRA_COMPILERS += updateqm
  }
}

unix:!macx:!haiku:!os2 {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  BINDIR = $$PREFIX/bin
  DATADIR =$$PREFIX/share

  DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

  #MAKE INSTALL

  target.path =$$BINDIR

  mime.path = $$DATADIR/mime/packages
  mime.files += ./data/$${TARGET}.xml

  desktop.path = $$DATADIR/applications
  desktop.files += ./data/$${TARGET}.desktop

  metainfo.path = $$DATADIR/metainfo
  metainfo.files += ./data/$${TARGET}.metainfo.xml

  appIcon.path = $$DATADIR/icons/hicolor/scalable/apps
  appIcon.files += ./data/$${TARGET}.svg

  fileIcon.path = $$DATADIR/icons/hicolor/scalable/mimetypes
  fileIcon.files += ./data/text-feathernotes-fnx.svg

  trans.path = $$DATADIR/feathernotes
  trans.files += data/translations/translations

  INSTALLS += target mime desktop metainfo appIcon fileIcon trans
}
else:macx {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /Applications/
  }
  BINDIR = $$PREFIX
  DATADIR = "$$BINDIR/$$TARGET".app

  DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

  #MAKE INSTALL

  target.path =$$BINDIR

  trans.path = $$DATADIR/feathernotes
  trans.files += data/translations/translations

  INSTALLS += target trans
}
else:haiku {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /boot/home/config/non-packaged/apps/FeatherNotes
  }
  BINDIR = $$PREFIX

  #MAKE INSTALL

  target.path =$$BINDIR

  trans.path = $$PREFIX
  trans.files += data/translations/translations

  INSTALLS += target trans
}
else:os2 {
  #VARIABLES
  RC_FILE = data/featherpad_os2.rc
  isEmpty(PREFIX) {
    PREFIX = /@unixroot/usr
  }
  BINDIR = $$PREFIX/bin
  DATADIR =$$PREFIX/share

  DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

  #MAKE INSTALL
  target.path =$$BINDIR

  help.path = $$DATADIR/featherpad
  help.files += ./data/help
  help.files += ./data/help_*

  trans.path = $$DATADIR/featherpad
  trans.files += data/translations/translations

  INSTALLS += target help trans
}
