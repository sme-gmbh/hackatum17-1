#-------------------------------------------------
#
# Project created by QtCreator 2017-11-19T00:33:19
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = hackatum17-1
TEMPLATE = app

CONFIG(debug, debug|release) {
  DEFINES += SUPERVERBOSE
  OBJECTS_DIR = .obj
  MOC_DIR = .moc
  RCC_DIR = .moc
  UI_DIR = .ui
} else {
  OBJECTS_DIR = .obj
  MOC_DIR = .moc
  RCC_DIR = .moc
  UI_DIR = .ui
}

SOURCES += main.cpp\
        mainwindow.cpp \
    imagetransform.cpp

HEADERS  += mainwindow.h \
    imagetransform.h

FORMS    += mainwindow.ui

RESOURCES += \
    icons.qrc
