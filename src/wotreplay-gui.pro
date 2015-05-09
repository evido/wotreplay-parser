#-------------------------------------------------
#
# Project created by QtCreator 2015-05-05T22:33:59
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = wotreplay-gui
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    imageviewer.cpp

HEADERS  += mainwindow.h \
    imageviewer.h

FORMS    += mainwindow.ui

win32: LIBS += -L$$PWD/../wotreplay-parser/build/lib/Release/ -LD:/Develop/wotreplay-parser/dependencies/x86/lib -LD:/Develop/wotreplay-parser/dependencies/x86/lib/VC -lwotreplay -ljsoncpp -llibeay32MT -lssleay32MT -lzlib -llibpng16 -llibxml2

INCLUDEPATH += $$PWD/../wotreplay-parser/src $$PWD/../wotreplay-parser/src
DEPENDPATH += $$PWD/../wotreplay-parser/src

win32:!win32-g++: PRE_TARGETDEPS += $$PWD/../wotreplay-parser/build/lib/Release/wotreplay.lib
else:win32-g++: PRE_TARGETDEPS += $$PWD/../wotreplay-parser/build/lib/Release/libwotreplay.a

win32: LIBS += -LD:/Develop/boost_1_58_0/lib/ -llibboost_system-vc120-mt-1_58

INCLUDEPATH += D:/Develop/boost_1_58_0
DEPENDPATH += D:/Develop/boost_1_58_0

win32:!win32-g++: PRE_TARGETDEPS += D:/Develop/boost_1_58_0/lib/libboost_system-vc120-mt-1_58.lib
else:win32-g++: PRE_TARGETDEPS += D:/Develop/boost_1_58_0/lib/libboost_system-vc120-mt-1_58.a
