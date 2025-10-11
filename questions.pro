QT       += core gui
QT += sql concurrent multimedia
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    core/fsrs.cpp \
    core/questionsql.cpp \
    answereditdialog.cpp \
    bindanswerdialog.cpp \
    descadddialog.cpp \
    dragqtreewidget.cpp \
    htmltableadddialog.cpp \
    htmltypstadddialog.cpp \
    learningdialog.cpp \
    main.cpp \
    mainwindow.cpp \
    questionmovedialog.cpp \
    questionrenamedialog.cpp \
    settimedialog.cpp \
    toolfunctions.cpp

HEADERS += \
    core/fsrs.h \
    core/questionsql.h \
    answereditdialog.h \
    bindanswerdialog.h \
    descadddialog.h \
    descradddialog.h \
    dragqtreewidget.h \
    htmltableadddialog.h \
    htmltypstadddialog.h \
    learningdialog.h \
    mainwindow.h \
    questionmovedialog.h \
    questionrenamedialog.h \
    settimedialog.h \
    toolfunctions.h

FORMS += \
    descadddialog.ui \
    htmltypstadddialog.ui \
    ui/answereditdialog.ui \
    ui/bindanswerdialog.ui \
    ui/htmltableadddialog.ui \
    ui/learningdialog.ui \
    ui/mainwindow.ui \
    ui/questionmovedialog.ui \
    ui/questionrenamedialog.ui \
    ui/settimedialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
