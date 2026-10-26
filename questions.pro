QT       += core gui
QT += sql concurrent multimedia
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to pass in order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the deprecated APIs in Qt 6.0.0

# 目录约定：
#   core/     核心数据与算法（FSRS、知识图谱库、题库）
#   parser/   知识笔记/语法解析
#   widgets/  可复用控件与树/表-视图辅助
#   dialogs/  全部对话框（.h/.cpp/.ui 同目录）
#   services/ 领域服务（跨视图的业务对象与持久化）
#   utils/    通用工具函数

# widgets/ 加入包含路径：mainwindow.ui 中 DragQTreeWidget 是 promoted widget，
# Qt Designer 每次保存 .ui 都会把 <header> 重写回裸文件名（去掉 widgets/ 前缀），
# uic 随之生成 #include <dragqtreewidget.h>；加入此目录后裸名也能解析，
# 不再因 Designer 保存 .ui 反复编译失败。源码 include 仍按约定使用跨目录相对路径。
INCLUDEPATH += $$PWD/widgets

SOURCES += \
    dialogs/clozemovedialog.cpp \
    main.cpp \
    mainwindow.cpp \
    mainwindow_answer.cpp \
    mainwindow_editor.cpp \
    mainwindow_knowledge.cpp \
    mainwindow_learn.cpp \
    mainwindow_preview.cpp \
    mainwindow_question.cpp \
    core/fsrs.cpp \
    core/knowledgedb.cpp \
    core/knowledgename.cpp \
    core/questionsql.cpp \
    parser/noteparser2.cpp \
    services/answerstore.cpp \
    services/questionpreview.cpp \
    services/typstimagecache.cpp \
    widgets/dragqtreewidget.cpp \
    widgets/knowledgetreehelper.cpp \
    widgets/typstrowdelegate.cpp \
    widgets/questiontableviewhelper.cpp \
    utils/toolfunctions.cpp \
    dialogs/answereditdialog.cpp \
    dialogs/bindanswerdialog.cpp \
    dialogs/descadddialog.cpp \
    dialogs/htmltableadddialog.cpp \
    dialogs/htmltypstadddialog.cpp \
    dialogs/learningdialog.cpp \
    dialogs/learningdialog_blank.cpp \
    dialogs/learningdialog_gate.cpp \
    dialogs/learningdialog_gate_view.cpp \
    dialogs/learningdialog_grid.cpp \
    dialogs/learningdialog_mask.cpp \
    dialogs/learningdialog_row.cpp \
    dialogs/learningdialog_submit.cpp \
    dialogs/phenadddialog.cpp \
    dialogs/questionmovedialog.cpp \
    dialogs/questionrenamedialog.cpp \
    dialogs/settimedialog.cpp

HEADERS += \
    dialogs/clozemovedialog.h \
    mainwindow.h \
    mainwindow_internal.h \
    core/fsrs.h \
    core/knowledgedb.h \
    core/questionsql.h \
    parser/noteparser2.h \
    services/answerstore.h \
    services/questionpreview.h \
    services/typstimagecache.h \
    widgets/dragqtreewidget.h \
    widgets/knowledgetreehelper.h \
    widgets/typstrowdelegate.h \
    widgets/questiontableviewhelper.h \
    utils/toolfunctions.h \
    dialogs/answereditdialog.h \
    dialogs/bindanswerdialog.h \
    dialogs/descadddialog.h \
    dialogs/htmltableadddialog.h \
    dialogs/htmltypstadddialog.h \
    dialogs/learningdialog.h \
    dialogs/learningdialog_internal.h \
    dialogs/phenadddialog.h \
    dialogs/questionmovedialog.h \
    dialogs/questionrenamedialog.h \
    dialogs/settimedialog.h

FORMS += \
    dialogs/clozemovedialog.ui \
    mainwindow.ui \
    dialogs/answereditdialog.ui \
    dialogs/bindanswerdialog.ui \
    dialogs/descadddialog.ui \
    dialogs/htmltableadddialog.ui \
    dialogs/htmltypstadddialog.ui \
    dialogs/learningdialog.ui \
    dialogs/phenadddialog.ui \
    dialogs/questionmovedialog.ui \
    dialogs/questionrenamedialog.ui \
    dialogs/settimedialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
