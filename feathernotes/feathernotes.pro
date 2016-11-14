QT += core gui \
      xml \
      widgets \
      printsupport \
      x11extras \
      svg

TARGET = feathernotes
TEMPLATE = app
CONFIG += c++11

SOURCES += main.cpp\
           fn.cpp \
           domitem.cpp \
           dommodel.cpp \
           lineedit.cpp \
           x11.cpp \
           textedit.cpp \
           simplecrypt.cpp \
           vscrollbar.cpp

HEADERS += fn.h \
           domitem.h \
           dommodel.h \
           textedit.h \
           lineedit.h \
           x11.h \
           spinbox.h \
           simplecrypt.h \
           vscrollbar.h \
           settings.h \
           help.h \
           filedialog.h \
           treeview.h \
           messagebox.h

FORMS += fn.ui \
         helpDialog.ui

RESOURCES += data/fn.qrc

unix:!macx: LIBS += -lX11

unix {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  BINDIR = $$PREFIX/bin
  DATADIR =$$PREFIX/share

  DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

  #MAKE INSTALL

  INSTALLS += target mime desktop appIcon fileIcon

  target.path =$$BINDIR

  mime.path = $$DATADIR/mime/packages
  mime.files += ./data/$${TARGET}.xml

  desktop.path = $$DATADIR/applications
  desktop.files += ./data/$${TARGET}.desktop

  appIcon.path = $$DATADIR/icons/hicolor/scalable/apps
  appIcon.files += ./data/$${TARGET}.svg

  fileIcon.path = $$DATADIR/icons/hicolor/scalable/mimetypes
  fileIcon.files += ./data/text-feathernotes-fnx.svg
}
