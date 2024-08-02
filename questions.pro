QT       += core gui
QT += sql concurrent multimedia
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    answereditdialog.cpp \
    fsrs.cpp \
    htmltableadddialog.cpp \
    learningdialog.cpp \
    main.cpp \
    mainwindow.cpp \
    questionmovedialog.cpp \
    questionsql.cpp \
    settimedialog.cpp \
    toolfunctions.cpp

HEADERS += \
    answereditdialog.h \
    fsrs.h \
    htmltableadddialog.h \
    learningdialog.h \
    mainwindow.h \
    questionmovedialog.h \
    questionsql.h \
    settimedialog.h \
    toolfunctions.h

FORMS += \
    answereditdialog.ui \
    htmltableadddialog.ui \
    learningdialog.ui \
    mainwindow.ui \
    questionmovedialog.ui \
    settimedialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target