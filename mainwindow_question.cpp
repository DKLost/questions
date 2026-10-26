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
const QString kQuestionHtmlPathTemplate = QStringLiteral("%1/data/%2/question.html");
const QString kFileExplorerProgram = QStringLiteral("explorer");
const QString kFileExplorerSelectArg = QStringLiteral("/select,");
const QString kQuestionIdExistsQuery = QStringLiteral("SELECT id FROM questions WHERE id = %1");
constexpr int kMaxHistoryLength = 100;
} // namespace

// question
void MainWindow::init_questionTableView()
{
    questionTableModel = new QSqlRelationalTableModel(this, questionSql->getDb());
    questionTableModel->setTable(kQuestionsTable);
    questionTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    questionTableModel->select();

    setupQuestionTableView(ui->questionTableView, questionTableModel);
    ui->questionTableView->installEventFilter(this);
}

int MainWindow::select_question_by_id(int id) // 选取问题，成功返回1，失败返回0
{
    if(id < 0)
        return 0;
    const int row = findQuestionRowGlobal(id);
    if(row < 0)
        return 0;
    const QModelIndex index = questionTableModel->index(row, QuestionIdColumn);
    ui->questionTableView->setCurrentIndex(index);
    ui->questionTableView->scrollTo(index, QAbstractItemView::EnsureVisible);
    on_questionTableView_activated(index);
    return 1;
}

// 当前范围未命中时回全表再找（id 跳转/历史前后跳是全局定位）
int MainWindow::findQuestionRowGlobal(int id)
{
    int row = findQuestionRowById(id);
    if(row < 0 && !scopeDerivedAbs.isEmpty())
    {
        setQuestionScope({});
        row = findQuestionRowById(id);
    }
    return row;
}

void MainWindow::selectQuestionRowAndActivate(int row)
{
    const QModelIndex target = questionTableModel->index(row, QuestionIdColumn); // 用可见列 id：abstractName 列隐藏，对隐藏索引 scrollTo 不生效
    ui->questionTableView->setCurrentIndex(target);
    ui->questionTableView->scrollTo(target, QAbstractItemView::EnsureVisible);
    on_questionTableView_activated(target); // 载题面 + 载答案（内部按行 id 取数，不依赖列）
}

void MainWindow::on_questionDirOpenButton_clicked()
{
    if(!ui->questionTableView->currentIndex().isValid())
        return;

    const QString path = kQuestionHtmlPathTemplate.arg(QDir::currentPath()).arg(currentQuestionId);
    QStringList args;
    args << kFileExplorerSelectArg << QDir::toNativeSeparators(path);
    QProcess process;
    process.startDetached(kFileExplorerProgram, args);
}

void MainWindow::on_questionTableView_clicked(const QModelIndex &index)
{
    on_questionTableView_activated(index);
}

void MainWindow::on_questionTableView_entered(const QModelIndex &index)
{
    if(QGuiApplication::mouseButtons() == Qt::NoButton)
        return;
    if(QGuiApplication::mouseButtons() == Qt::RightButton)
        return;
    on_questionTableView_activated(index);
}

// 原子换绑：屏蔽期内置新 id 并替换内容，丢弃 setHtml 触发的 textChanged，确保内容与 editorQuestionId 同题
void MainWindow::loadQuestionIntoEditor(int questionId)
{
    leaveQuestionPreview(); // 换题前退出预览：先还原旧题真实题面（随即被新题覆盖）
    const QString questionHTML = questionSql->read_questionHTML(questionId);
    const QSignalBlocker blocker(ui->questionTextEdit);
    editorQuestionId = questionId;
    ui->questionTextEdit->setHtml(questionHTML);
}

void MainWindow::on_questionTableView_activated(const QModelIndex &index)
{
    if(!index.isValid()) // 键盘越界/模型重置残留：切题会导致 currentQuestionId=0 错位
        return;
    const int newQuestionId = index.siblingAtColumn(QuestionIdColumn).data().toInt();
    if(newQuestionId <= 0)
        return;
    setQuestionAreasEnabled(true); // 选中题目：恢复 html/cloze 编辑区（变灰态由此解除）
    if(newQuestionId == currentQuestionId && newQuestionId == editorQuestionId) // 同一题不重复装载
    {
        ui->questionTableView->setCurrentIndex(index);
        return;
    }

    const bool keepPreview = ui->questionPrevieButton->isChecked(); // 预览是"模式"：换题后新题继续显示预览
    saveCurrentQuestionContent(); // ① 先保存当前正在编辑的内容（此刻编辑器=editorQuestionId 的题）
    setCurrentQId(newQuestionId);
    loadQuestionIntoEditor(newQuestionId); // ② 原子换绑
    ui->questionTableView->setCurrentIndex(index);
    load_answer(newQuestionId);

    select_knowledgeRowByDerivedAbs(index.siblingAtColumn(QuestionAbstractNameColumn).data().toString()); // 反向联动：索引区展开并选中所属概念行

    if(keepPreview) // ③ 重新勾选 → enterQuestionPreview：备份新题真实题面并填满全部答案（仍只读）
        ui->questionPrevieButton->setChecked(true);
}

// 题目 → 索引区：先在主树里选该现象行；主树没有它的行时（当前是检索结果视图、或该行只以别支形式存在），
// 逻辑上等价于 ctrl+左键它的"顶层宾词"行——打开宾词视图并在其中选中该现象行（见 docs/01 §3.4）。
void MainWindow::select_knowledgeRowByDerivedAbs(const QString &derivedAbs)
{
    if(selectKnowledgeRowByDerivedAbs(ui->phenTreeView, derivedAbs))
        return;
    const QString word = topLevelObjectWordOfDerivedAbs(derivedAbs);
    if(word.isEmpty())
        return; // 挂载键已失效（找不到所属顶层宾词）：不动视图，避免乱跳
    showPhenObjTree(word); // ＝ ctrl+左键：宾词视图以该词面作顶层宾词，命中行及其以下整棵子树都在里面
    selectKnowledgeRowByDerivedAbs(ui->phenObjTreeView, derivedAbs);
}

void MainWindow::on_questionAddButton_clicked()
{
    const int id = questionSql->get_max_id(kQuestionsTable) + 1;
    const QString name = ui->lineEdit->text();
    QTreeView *view = activePhenView(); // 挂到最后聚焦的知识视图（主树 / 宾词视图）的选中概念；无选中写 NULL，与点击跳转归属一致
    const QString abstractName = questionDerivedAbsForIndex(view, view->currentIndex());
    questionSql->add_question(id, name, abstractName);
    if(!abstractName.isEmpty()) // 仅新建时用概念末段名预置标题，此后由编辑器内容覆盖
        questionSql->write_questionHTML(id, QStringLiteral("<h1>%1</h1>")
                                                .arg(KnowledgeDB::lastObjectOfDerivedAbs(abstractName).toHtmlEscaped()));

    rebuildQuestionFilter(); // 原生 SQL 插入模型不感知，须按当前范围/名称条件重取

    const int row = findQuestionRowGlobal(id); // 视图可排序故按 id 找行；挂到范围外概念时回全表
    if(row < 0)
        return;

    ui->questionTableView->selectRow(row);
    ui->questionTableView->scrollTo(ui->questionTableView->currentIndex());
    on_questionTableView_clicked(ui->questionTableView->currentIndex());
    refreshAllDueDaysColumns(); // 新题挂了概念：知识树「到期」列随之出现天数
}

bool MainWindow::confirmDeleteQuestion()
{
    return QMessageBox::warning(this, QStringLiteral("删除确认"),
                                QStringLiteral("即将删除该题目及其文件夹中所有内容\n"
                                               "确定要删除吗?"),
                                QMessageBox::Yes | QMessageBox::No)
           != QMessageBox::No;
}

void MainWindow::clearQuestionEditor()
{
    leaveQuestionPreview(); // 预览态下解绑：先还原真实题面，再原子清空
    const QSignalBlocker blocker(ui->questionTextEdit); // 屏蔽信号后断开 id 并清空，丢弃 clear 触发的 textChanged
    editorQuestionId = -1;
    ui->questionTextEdit->clear();
}

// 无选中题（启动初始态、点了纯概念行、删空最后一题）时把 html 题面编辑器与 cloze 答案区整体禁用：
// 变灰、不可编辑、不接受交互；有题时（on_questionTableView_activated）再整体恢复
void MainWindow::setQuestionAreasEnabled(bool enabled)
{
    ui->questionTextEdit->setEnabled(enabled);  // html 编辑区
    ui->answerTreeWidget->setEnabled(enabled);  // cloze 答案区
    ui->answerAddButton->setEnabled(enabled);
    ui->answerDelButton->setEnabled(enabled);
    ui->answerGenAddButton->setEnabled(enabled);
    ui->clozeMovePushButton->setEnabled(enabled);
}

void MainWindow::selectQuestionRowAfterDelete(int row)
{
    const int rowCount = questionTableModel->rowCount();
    if(rowCount <= 0)
    {
        showNoQuestionForConcept(); // 最后一题已删：概念呈现区回到"无题"的禁用空态
        return;
    }
    row = qBound(0, row, rowCount - 1); // 删除前记下位置，删完选中同位置（末尾则上移一行）
    ui->questionTableView->selectRow(row);
    on_questionTableView_clicked(ui->questionTableView->currentIndex());
}

void MainWindow::on_questionDelButton_clicked()
{
    if(!confirmDeleteQuestion())
        return;
    const QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid()) // 概念无题目时可能无当前行
        return;
    const int id = index.siblingAtColumn(QuestionIdColumn).data().toInt();
    const int row = index.row();
    ui->questionTableView->clearSelection();
    clearQuestionEditor();
    questionSql->del_question(id);

    questionTableModel->select(); // 原生 SQL 删除模型不感知，须重取数（否则被删行仍显示）
    selectQuestionRowAfterDelete(row);
    refreshAllDueDaysColumns(); // 被删题的概念不再有天数（否则会留下"有数字但无题"的假象）
}

// 只改 questions.abstractName，不动图谱
void MainWindow::on_questionMoveButton_clicked()
{
    const QModelIndex cur = ui->questionTableView->currentIndex();
    if(!cur.isValid())
    {
        QMessageBox::information(this, QStringLiteral("移动题目"), QStringLiteral("请先在题目表中选择要移动的题目。"));
        return;
    }
    const int questionId = cur.siblingAtColumn(QuestionIdColumn).data().toInt();
    const QString oldDerivedAbs = questionSql->get_value(kQuestionsTable, questionId, kAbstractNameField).toString();

    const QString newDerivedAbs = pickTargetConcept(oldDerivedAbs, QStringLiteral("移动题目"), false, false); // 题目只有挂载键，不涉及成为顶层
    if(newDerivedAbs.isEmpty() || newDerivedAbs == oldDerivedAbs)
        return;

    questionSql->set_value(kQuestionsTable, questionId, kAbstractNameField, newDerivedAbs);

    rebuildQuestionFilter(); // 保留当前范围/名称条件；新位置在范围外则回全表
    const int row = findQuestionRowGlobal(questionId);
    if(row >= 0)
    {
        const QModelIndex target = questionTableModel->index(row, QuestionIdColumn);
        ui->questionTableView->setCurrentIndex(target);
        ui->questionTableView->scrollTo(target, QAbstractItemView::EnsureVisible);
    }

    select_knowledgeRowByDerivedAbs(newDerivedAbs); // 检索视图不含该概念时不命中，保持原视图
    refreshAllDueDaysColumns(); // 改挂后：旧概念不再显示天数、新概念开始显示
}

void MainWindow::on_questionTextEdit_textChanged()
{
    saveCurrentQuestionContent();
}

void MainWindow::on_questionRenameButton_clicked()
{
    saveCurrentQuestionContent(); // 先保存当前编辑的内容

    const QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    const int questionId = index.siblingAtColumn(QuestionIdColumn).data().toInt();
    questionRenameDialog.setText(index.siblingAtColumn(QuestionNameColumn).data().toString());
    if(questionRenameDialog.exec())
    {
        questionTableModel->setData(index.siblingAtColumn(QuestionNameColumn), questionRenameDialog.getText());
        // OnManualSubmit 下 submitAll 成功后模型会重新 select（QSqlTableModel 文档：视图会丢失选择），
        // 当前行随之失效 —— 右侧编辑区会解绑，加/删/改答案与答案编辑全部在 currentIndex 守卫处静默失效。
        // 故按 id 回选并重新激活本题（同题不会重载题面，答案与编辑内容保持不动）
        questionTableModel->submitAll();
        select_question_by_id(questionId);
    }
}

void MainWindow::on_questionSearchButton_clicked()
{
    saveCurrentQuestionContent(); // 先保存当前编辑的内容

    rebuildQuestionFilter(); // 名称与 scopeDerivedAbs 范围 AND 叠加；搜索框清空即仅按范围过滤
    const Qt::MatchFlags matchTypes[] = {Qt::MatchExactly, Qt::MatchStartsWith, Qt::MatchContains};
    for(const Qt::MatchFlags &matchType : matchTypes)
    {
        const QModelIndexList hits = questionTableModel->match(questionTableModel->index(0, QuestionNameColumn),
                                                               Qt::DisplayRole, ui->lineEdit->text(), 1, matchType);
        if(hits.isEmpty())
            continue;
        const QModelIndex index = *hits.begin();
        ui->questionTableView->setCurrentIndex(index);
        ui->questionTableView->scrollTo(index, QAbstractItemView::EnsureVisible);
        on_questionTableView_activated(index);
        break;
    }
}

void MainWindow::on_idLineEdit_returnPressed()
{
    saveCurrentQuestionContent(); // 先保存当前编辑的内容

    const int questionId = ui->idLineEdit->text().toInt();
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.exec(kQuestionIdExistsQuery.arg(questionId));
    if(query.next())
        select_question_by_id(questionId); // 分类跳转已移除，直接定位题目
}

// 落地题恒定于 editorQuestionId：换题/清空均原子换绑，进入时内容与 id 必同属一题
void MainWindow::saveCurrentQuestionContent()
{
    leaveQuestionPreview(); // 预览内容不是真实题面：先还原，落盘的内容恒为真实题面
    if(editorQuestionId <= 0)
        return;
    questionSql->write_questionHTML(editorQuestionId, ui->questionTextEdit->toHtml());
    questionSql->write_questionMd(editorQuestionId, ui->questionTextEdit->toMarkdown());
}

void MainWindow::set_and_jump_to_qId(int questionId)
{
    ui->idLineEdit->setText(QString::number(questionId));
    on_idLineEdit_returnPressed();
}

void MainWindow::on_parserButton_clicked()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("parser"));
    auto *layout = new QVBoxLayout(&dlg);
    auto *edit = new QPlainTextEdit(&dlg);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(edit);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if(dlg.exec() == QDialog::Accepted)
    {
        NoteParser parser;
        parser.parse(edit->toPlainText());
    }
}

void MainWindow::on_lineEdit_returnPressed()
{
    on_questionSearchButton_clicked();
}

void MainWindow::setCurrentQId(int questionId)
{
    currentQuestionId = questionId;
    ui->idLineEdit->setText(QString::number(questionId));
    if(qIdHistoryPos == 0 ||
       questionId != qIdHistoryQueue[qIdHistoryPos - 1])
    {
        qIdHistoryQueue.insert(qIdHistoryPos, questionId);
        qIdHistoryPos = qIdHistoryPos >= kMaxHistoryLength ? kMaxHistoryLength : qIdHistoryPos + 1;
        if(qIdHistoryQueue.size() > kMaxHistoryLength)
            qIdHistoryQueue.dequeue();
    }
}

void MainWindow::on_prevQButton_clicked()
{
    if(qIdHistoryPos == 0)
        return;
    qIdHistoryPos = qIdHistoryPos <= 1 ? 1 : qIdHistoryPos - 1;
    ui->idLineEdit->setText(QString::number(qIdHistoryQueue[qIdHistoryPos - 1]));
    on_idLineEdit_returnPressed();
}

void MainWindow::on_nextQButton_clicked()
{
    if(qIdHistoryPos == 0)
        return;
    qIdHistoryPos = qIdHistoryPos >= qIdHistoryQueue.size() ? qIdHistoryQueue.size() : qIdHistoryPos + 1;
    ui->idLineEdit->setText(QString::number(qIdHistoryQueue[qIdHistoryPos - 1]));
    on_idLineEdit_returnPressed();
}
