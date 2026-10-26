#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mainwindow_internal.h"

#include "parser/noteparser2.h"
#include "core/knowledgedb.h"
#include "widgets/knowledgetreehelper.h"
#include "widgets/questiontableviewhelper.h"
#include "dialogs/phenadddialog.h"
#include "utils/toolfunctions.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QSettings>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QSize>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardItem>
#include <QStringList>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextFrameFormat>
#include <QTextImageFormat>
#include <QTextList>
#include <QTextListFormat>
#include <QTextTableFormat>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const QString kQuestionIdFilterTemplate = QStringLiteral("questions.id = %1");
} // namespace

// 折叠态行按 is 子行解析，与点击跳转同一规则
void MainWindow::pushKnowledgeSyncToLearningDialog()
{
    const QString derivedAbs = questionDerivedAbsForIndex(activePhenView(), activePhenView()->currentIndex());
    learningDialog->setKnowledgeViewSync(findObjsActive,
                                         ui->subjLineEdit->text(),
                                         ui->predLineEdit->text(),
                                         ui->objLineEdit->text(),
                                         collectExpandedPhenIds(),
                                         derivedAbs);
}

// learn
// 分类功能已移除，学完仅刷新题目表
void MainWindow::refreshAfterLearning(bool fullReload)
{
    questionTableModel->select();
    refreshAllDueDaysColumns(); // 复习改了各题 nextDate：只重写知识树的"到期"列天数，不动展开与选中
    if(learningDialog->getLastId() != -1)
    {
        if(fullReload)
            select_question_by_id(learningDialog->getLastId());
        else
            set_and_jump_to_qId(learningDialog->getLastId());
    }
}

void MainWindow::on_itemLearnButton_clicked()
{
    saveCurrentQuestionContent(); // 先保存当前编辑的内容

    const QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    const int id = index.siblingAtColumn(QuestionIdColumn).data().toInt();

    learningDialog->clear_question_display();
    pushKnowledgeSyncToLearningDialog();
    learningDialog->set_items_table(kQuestionIdFilterTemplate.arg(id));
    learningDialog->setOnlyToLearn(false);
    learningDialog->exec();

    refreshAfterLearning(true);
}

void MainWindow::on_speedLearnButton_clicked() // 标签学习（不重复）
{
    saveCurrentQuestionContent(); // 先保存当前编辑的内容

    learningDialog->setOnlyToLearn(false);
    learningDialog->setIsSpeedLearn(true);
    pushKnowledgeSyncToLearningDialog();
    learningDialog->set_items_table(questionTableModel->filter());
    learningDialog->exec();
    learningDialog->setIsSpeedLearn(false);

    refreshAfterLearning(true);
}

void MainWindow::on_categoryToLearnButton_clicked() // 表项学习（仅待学）
{
    saveCurrentQuestionContent(); // 先保存当前编辑的内容

    learningDialog->clear_question_display();
    pushKnowledgeSyncToLearningDialog();
    learningDialog->set_items_table(questionSql->get_toLearn_condString(questionTableModel->filter()));
    learningDialog->setOnlyToLearn(true);
    learningDialog->exec();

    refreshAfterLearning(false);
}

void MainWindow::on_categoryLearnButton_clicked() // 表项学习（全部）
{
    saveCurrentQuestionContent(); // 先保存当前编辑的内容

    pushKnowledgeSyncToLearningDialog();
    learningDialog->set_items_table(questionTableModel->filter());
    learningDialog->setOnlyToLearn(false);
    learningDialog->exec();

    refreshAfterLearning(true);
}
