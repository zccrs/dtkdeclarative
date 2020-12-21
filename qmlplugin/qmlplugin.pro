TEMPLATE = lib
TARGET = dtkdeclarativeplugin
QT += qml quick dtkcore dtkgui

unix: LIBS += -L$$OUT_PWD/../src -ldtkdeclarative

CONFIG(debug, debug|release) {
    unix: QMAKE_RPATHDIR += $$OUT_PWD/../src
}

CONFIG += plugin c++11

TARGET = $$qtLibraryTarget($$TARGET)
uri = com.deepin.dtk

INCLUDEPATH += $$PWD/../src

# Input
SOURCES += \
        qmlplugin_plugin.cpp

HEADERS += \
        qmlplugin_plugin.h

DISTFILES = qmldir

!equals(_PRO_FILE_PWD_, $$OUT_PWD) {
    copy_qmldir.target = $$OUT_PWD/qmldir
    copy_qmldir.depends = $$_PRO_FILE_PWD_/qmldir
    copy_qmldir.commands = $(COPY_FILE) \"$$replace(copy_qmldir.depends, /, $$QMAKE_DIR_SEP)\" \"$$replace(copy_qmldir.target, /, $$QMAKE_DIR_SEP)\"
    QMAKE_EXTRA_TARGETS += copy_qmldir
    PRE_TARGETDEPS += $$copy_qmldir.target
}

qmldir.files = qmldir
unix {
    installPath = $$[QT_INSTALL_QML]/$$replace(uri, \\., /)
    qmldir.path = $$installPath
    target.path = $$installPath
    INSTALLS += target qmldir
}