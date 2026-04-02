QT       += core gui widgets network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    dataparser.cpp \
    chatmodel.cpp \
    ipcworker.cpp

HEADERS += \
    mainwindow.h \
    constants.h \
    chatmessage.h \
    dataparser.h \
    chatmodel.h \
    ipcworker.h

FORMS += \
    mainwindow.ui

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc
