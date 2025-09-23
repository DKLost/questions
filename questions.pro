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
    dragqtreewidget.cpp \
    htmltableadddialog.cpp \
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
    dragqtreewidget.h \
    htmltableadddialog.h \
    learningdialog.h \
    mainwindow.h \
    questionmovedialog.h \
    questionrenamedialog.h \
    settimedialog.h \
    third_party/cppjieba/DictTrie.hpp \
    third_party/cppjieba/FullSegment.hpp \
    third_party/cppjieba/HMMModel.hpp \
    third_party/cppjieba/HMMSegment.hpp \
    third_party/cppjieba/Jieba.hpp \
    third_party/cppjieba/KeywordExtractor.hpp \
    third_party/cppjieba/MPSegment.hpp \
    third_party/cppjieba/MixSegment.hpp \
    third_party/cppjieba/PosTagger.hpp \
    third_party/cppjieba/PreFilter.hpp \
    third_party/cppjieba/QuerySegment.hpp \
    third_party/cppjieba/SegmentBase.hpp \
    third_party/cppjieba/SegmentTagged.hpp \
    third_party/cppjieba/TextRankExtractor.hpp \
    third_party/cppjieba/Trie.hpp \
    third_party/cppjieba/Unicode.hpp \
    third_party/limonp/ArgvContext.hpp \
    third_party/limonp/Closure.hpp \
    third_party/limonp/Colors.hpp \
    third_party/limonp/Condition.hpp \
    third_party/limonp/Config.hpp \
    third_party/limonp/ForcePublic.hpp \
    third_party/limonp/LocalVector.hpp \
    third_party/limonp/Logging.hpp \
    third_party/limonp/NonCopyable.hpp \
    third_party/limonp/StdExtension.hpp \
    third_party/limonp/StringUtil.hpp \
    toolfunctions.h

FORMS += \
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

DISTFILES += \
