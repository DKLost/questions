#include "learningdialog.h"
#include "ui_learningdialog.h"
#include "learningdialog_internal.h"
#include "utils/toolfunctions.h"
#include "core/fsrs.h"
#include "core/knowledgedb.h"
#include "widgets/knowledgetreehelper.h"
#include "widgets/questiontableviewhelper.h"
#include "services/typstimagecache.h"
#include <QSet>
#include <QImage>
#include <QUrl>
#include <QPainter>
#include <QTextImageFormat>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <QSqlError>
#include <QToolButton>
#include <QPushButton>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTextDocument>
#include <QTextBlock>
#include <algorithm>
#include <functional>
#include <QTextFragment>
#include <QTextDocumentFragment>

using namespace learningdialog_internal;

namespace {
// 计时与时间格式
constexpr int kTimerTickMs = 10; // 计时器步进，也是作答时间的显示精度

// 排版与配色
const QStringList kUiFontFamilies = {"IBM Plex Mono", "IBM Plex Math", "Noto Sans Mono",
                                     "Microsoft YaHei UI", "Microsoft YaHei", "Arial"};
constexpr int kUiFontSize = 12;
constexpr double kIndentWidth = 32.5; // 题面固定缩进

constexpr double kRemainingTimeFactor = 0.87;     // 预估总时长按 87% 折算（留出复习损耗）

// 提交后切题与默认排序（读题目表状态列与后续题学习量）
const QString kStateField = QStringLiteral("state");
const QString kLearningState = QStringLiteral("learning");
const QString kNewState = QStringLiteral("new");
constexpr int kMaxOrderGapSeconds = 30; // 默认排序：向前累计学习时长不超过 30s 的题目之间插入
}

// 提交后的下一次点击：按排序模式维护 orderNum 并切到下一题（当前题仍在表内则前进一行）
void LearningDialog::advanceToNextQuestion()
{
    int newCurrentId = questionTableModel->index(lastAnsweredRowIndex, 0).data().toInt();
    if(newCurrentId == currentQuestionId)
    {
        const QString sortMode = ui->comboBox->currentText();
        bool orderChanged = false;
        if(sortMode == kRandomSortMode)
        {
            questionSql->set_value(kQuestionsTable, newCurrentId, kOrderNumField,
                                   QRandomGenerator::global()->bounded(kRandomOrderMax));
            orderChanged = true;
        } else if(sortMode == kDefaultSortMode)
        {
            orderChanged = rescheduleAnsweredRowForDefaultOrder();
        }
        if(orderChanged)
        {
            reloadQuestionTable();
            if(questionTableModel->rowCount() == 0)
                return;
            newCurrentId = questionTableModel->index(lastAnsweredRowIndex, 0).data().toInt();
        }
    }
    int newRow = lastAnsweredRowIndex;
    if(newCurrentId == currentQuestionId) // 同一题：前进一行（到底回卷首行）
    {
        newRow = lastAnsweredRowIndex + 1;
        if(newRow >= questionTableModel->rowCount())
            newRow = 0;
    }
    ui->tableView->selectRow(newRow);
    set_question(ui->tableView->currentIndex().siblingAtColumn(0).data().toInt());
}

// 默认排序：把刚提交的未学完题目插到"约 30s 学习量"之后，避免它总排在最前；返回是否改动了排序
bool LearningDialog::rescheduleAnsweredRowForDefaultOrder()
{
    const QString state = questionTableModel->index(lastAnsweredRowIndex, QuestionSql::headerColumnMap[kStateField]).data().toString();
    if(state != kLearningState && state != kNewState)
        return false;
    int accumulatedSeconds = 0;
    int row = lastAnsweredRowIndex + 1;
    while(accumulatedSeconds < kMaxOrderGapSeconds && row < questionTableModel->rowCount())
    {
        const QTime rowGoodTime = QTime::fromString(
            questionTableModel->index(row, QuestionSql::headerColumnMap[kGoodTimeField]).data().toString(), kGoodTimeFormat);
        accumulatedSeconds += rowGoodTime.msecsSinceStartOfDay() / 1000; // 累计的是后续各题的学习量
        ++row;
    }
    const int prevOrderNum = questionTableModel->index(row - 1, QuestionSql::headerColumnMap[kOrderNumField]).data().toInt();
    const int nextOrderNum = row < questionTableModel->rowCount()
                                 ? questionTableModel->index(row, QuestionSql::headerColumnMap[kOrderNumField]).data().toInt()
                                 : kMaxOrderNum;
    questionSql->set_value(kQuestionsTable, currentQuestionId, kOrderNumField, (prevOrderNum + nextOrderNum) / 2);
    return true;
}

LearningDialog::LearningDialog(QuestionSql *sqlConnection, QWidget *parent)
    : QDialog(parent), ui(new Ui::LearningDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    timer = new QTimer(this); // 当前题作答计时
    timer->setInterval(kTimerTickMs);
    connect(timer, &QTimer::timeout, this, &LearningDialog::timerHandler);

    totalTimer = new QTimer(this); // 本次复习总计时
    totalTimer->setInterval(kTimerTickMs);
    connect(totalTimer, &QTimer::timeout, this, &LearningDialog::totalTimerHandler);

    answerElapsed.setHMS(0, 0, 0);
    questionSql = sqlConnection;
    answerStore = new AnswerStore(questionSql);
    fontMetrics = new QFontMetrics(QApplication::font());
    currentQuestionId = -1;
    answerStage = AnswerStage::Waiting;
    isSpeedLearn = false;
    onlyToLearn = false;
    totalCount = 0;
    correctCount = 0;
    answerRowCount = 0;
    answerElapsedLabel = nullptr;
    currentLineEdit = nullptr;
    lastAnsweredRowIndex = 0;
    lastReviewedQuestionId = -1;
    totalRemainingTimeMsec = 0;

    questionTableModel = new QSqlRelationalTableModel(this, questionSql->getDb()); // 题目表
    questionTableModel->setTable(QStringLiteral("questions"));
    questionTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    setupQuestionTableView(ui->tableView, questionTableModel);

    initKnowledgeTree();
    initSubmitShortcut();
    ui->tableView->installEventFilter(this);                // Tab：从题目表跳到首个空输入框
    ui->textBrowser->installEventFilter(this);              // 题面点空位：定位到对应答案行
    ui->textBrowser->viewport()->installEventFilter(this);  // 鼠标事件落在 viewport 上（须装此处才收得到）

    const QFont readingFont(kUiFontFamilies, kUiFontSize); // 题面浏览器字体与缩进
    ui->textBrowser->setFont(readingFont);
    ui->textBrowser->setTabStopDistance(ui->textBrowser->fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 2);
    ui->textBrowser->document()->setIndentWidth(kIndentWidth);

    typstWatchProcess.setProcessChannelMode(QProcess::MergedChannels); // typst 预览进程
    connect(&typstWatchProcess, &QProcess::readyReadStandardOutput, this, &LearningDialog::onTypstWatcher_standard_output);
    // 链卡(typst)行的标准答案图是异步编译的：落盘后把原文答案换成公式图
    connect(TypstImageCache::instance(), &TypstImageCache::imageReady, this, &LearningDialog::refreshChainTypstAnswerLabels);
}

// 知识树（与主界面知识索引区同渲染；复习时只读，仅显示与按题目定位）
void LearningDialog::initKnowledgeTree()
{
    knowledgeModel = new QStandardItemModel(this);
    ui->phenTreeView->setModel(knowledgeModel);
    installReadOnlyKnowledgeTree(ui->phenTreeView);
    installCollapseMergeOnLeave(ui->phenTreeView); // 离开 is 子行自动折叠 ⇒ 父行（与主界面一致）
    ui->phenTreeView->viewport()->installEventFilter(this); // 门控行展开锁定守卫（须后装先执行）
    ui->phenTreeView->installEventFilter(this);
    connect(ui->phenTreeView, &QTreeView::expanded, this, [this](const QModelIndex &index) {
        if(isDerivedAbsLocked(index)) // 兜底：任何路径展开锁行时立即收回
            ui->phenTreeView->collapse(index);
    });

    // 宾词视图：默认空，只在"反选兜底"（主树里没有该现象行）时按顶层宾词填充——与主界面同口径
    phenObjModel = new QStandardItemModel(this);
    ui->phenObjTreeView->setModel(phenObjModel);
    installReadOnlyKnowledgeTree(ui->phenObjTreeView);
    installCollapseMergeOnLeave(ui->phenObjTreeView);
}

// 回车统一走「提交」（答题/切题），不触发知识树搜索（检索只由 🔍 点击触发）
void LearningDialog::initSubmitShortcut()
{
    ui->pushButton->setDefault(true);
    ui->findObjsPushButton->setAutoDefault(false);
}

LearningDialog::~LearningDialog()
{
    delete knowledgeRoot;
    delete phenObjKnowledgeRoot;
    delete ui;
}

void LearningDialog::start_timer()
{
    answerElapsed.setHMS(0, 0, 0);
    answerStage = AnswerStage::Waiting;
    timer->start();
}

void LearningDialog::stop_timer()
{
    timer->stop();
}

void LearningDialog::timerHandler()
{
    answerElapsed = answerElapsed.addMSecs(kTimerTickMs);
    if(answerElapsedLabel)
        answerElapsedLabel->setText(ToolFunctions::answerTimeText(answerElapsed));

    const int row = focusedAnswerRow();
    if(row < 0)
        return;
    QLabel *rowTimeLabel = answerRowAt(row).timeLabel;
    if(rowTimeLabel)
        rowTimeLabel->setText(ToolFunctions::answerTimeText(
            ToolFunctions::msz2QTime(rowTimeLabel->text()).addMSecs(kTimerTickMs)));
}

void LearningDialog::totalTimerHandler()
{
    sessionElapsed = sessionElapsed.addMSecs(kTimerTickMs);
    ui->totalTimeLabel->setText(ToolFunctions::sessionTimeText(sessionElapsed));

    totalRemainingTimeMsec -= kTimerTickMs; // 剩余时间可为负：超时后以 "-" 前缀显示超出量
    const QTime remaining = QTime::fromMSecsSinceStartOfDay(qAbs(totalRemainingTimeMsec));
    ui->remainingTimeLabel->setText((totalRemainingTimeMsec >= 0 ? QString() : QStringLiteral("-"))
                                    + ToolFunctions::sessionTimeText(remaining));
}

int LearningDialog::getLastId() const
{
    return lastReviewedQuestionId;
}

void LearningDialog::setIsSpeedLearn(bool enabled)
{
    isSpeedLearn = enabled;
}

void LearningDialog::setOnlyToLearn(bool enabled)
{
    onlyToLearn = enabled;
}

// 焦点在答案输入框且属于本对话框时返回其行号，否则 -1（避免误取知识树搜索框的行号）
int LearningDialog::focusedAnswerRow() const
{
    QLineEdit *activeInput = ToolFunctions::get_active_LineEdit();
    if(!activeInput || !activeInput->parentWidget()
       || activeInput->parentWidget()->layout() != ui->groupBox->layout())
        return -1;
    return activeInput->toolTip().toInt();
}

QGridLayout *LearningDialog::answerGrid() const
{
    return qobject_cast<QGridLayout *>(ui->groupBox->layout());
}

// 取网格中一行的控件集合；空位（未要复习的答案行）返回空控件
LearningDialog::AnswerRowControls LearningDialog::answerRowAt(int row) const
{
    AnswerRowControls controls;
    QGridLayout *layout = answerGrid();
    if(!layout || row < 0 || row >= answerRowCount)
        return controls;
    auto widgetAt = [layout, row](int column) -> QWidget *
    {
        QLayoutItem *item = layout->itemAtPosition(row, column);
        return item ? item->widget() : nullptr;
    };
    controls.lineNumber = qobject_cast<QLabel *>(widgetAt(ColumnLineNumber));
    if(QLayoutItem *inputItem = layout->itemAtPosition(row, ColumnInput))
    {
        controls.compareBox = qobject_cast<QVBoxLayout *>(inputItem->layout());
        if(controls.compareBox) // 参与作答：纵向容器 = [输入行, 对照行]
        {
            if(controls.compareBox->count() > 0)
                controls.inputBox = qobject_cast<QHBoxLayout *>(controls.compareBox->itemAt(0)->layout());
            if(controls.compareBox->count() > 1)
                controls.belowBox = qobject_cast<QHBoxLayout *>(controls.compareBox->itemAt(1)->layout());
        }
        else if(QLabel *label = qobject_cast<QLabel *>(inputItem->widget()))
            controls.answerLabel = label; // 未到期的空：答案文本直接占在输入列（不参与作答）
    }
    if(controls.inputBox && controls.inputBox->count() > 1)
    {
        controls.input = qobject_cast<QLineEdit *>(controls.inputBox->itemAt(0)->widget());
        controls.typstPreview = qobject_cast<QLabel *>(controls.inputBox->itemAt(1)->widget());
    }
    if(controls.belowBox)
    {
        // 对照行：typst 行 [0]=原文、[1]=答案；其余行只有 [0]=答案（非 typst 行不放占位标签，免得隐藏件仍按最小尺寸参与布局把答案推右）
        const int slot = answerSlotIndexOf(row);
        if(slot > 0 && controls.belowBox->count() > 0)
            controls.answerSourceLabel = qobject_cast<QLabel *>(controls.belowBox->itemAt(0)->widget());
        if(controls.belowBox->count() > slot)
            controls.answerLabel = qobject_cast<QLabel *>(controls.belowBox->itemAt(slot)->widget());
    }
    controls.checkBox = qobject_cast<QCheckBox *>(widgetAt(ColumnCheck));
    controls.timeLabel = qobject_cast<QLabel *>(widgetAt(ColumnTime));
    controls.goodTimeLabel = qobject_cast<QLabel *>(widgetAt(ColumnGoodTime));
    controls.ratingBox = widgetAt(ColumnRating);
    return controls;
}

// 该行是否参与作答：输入列是"输入行 + 对照行"的纵向容器（未到期的空只有答案文本这一件控件）
bool LearningDialog::rowParticipates(int row) const
{
    QGridLayout *layout = answerGrid();
    QLayoutItem *inputItem = layout ? layout->itemAtPosition(row, ColumnInput) : nullptr;
    return inputItem && qobject_cast<QVBoxLayout *>(inputItem->layout());
}

// 输入框正下方的对照行布局（未参与作答的行返回空）
QHBoxLayout *LearningDialog::answerBelowBox(int row) const
{
    QGridLayout *layout = answerGrid();
    QLayoutItem *inputItem = layout ? layout->itemAtPosition(row, ColumnInput) : nullptr;
    QVBoxLayout *compareBox = inputItem ? qobject_cast<QVBoxLayout *>(inputItem->layout()) : nullptr;
    return compareBox && compareBox->count() > 1 ? qobject_cast<QHBoxLayout *>(compareBox->itemAt(1)->layout()) : nullptr;
}

// 对照行里"标准答案"所占下标：typst 行先把"原文"摆在输入框正下方，答案图位于预览下方（下标 1）；其余行答案就在下标 0
int LearningDialog::answerSlotIndexOf(int row) const
{
    if(row < 0 || row >= currentAnswers.count())
        return 0;
    const QString type = currentAnswers[row].toObject().value(AnswerStore::FieldType).toString();
    return (type == AnswerStore::TypeAutoTypst || type == AnswerStore::TypeAutoChainTypst) ? 1 : 0;
}

// 揭示时把对照标签按输入框内容区的左内缩对齐：输入框文字起点 = 边框 + 内部留白，标签默认从控件左缘起排，不补这段会差 1~3px
void LearningDialog::alignCompareLabels(const AnswerRowControls &controls)
{
    if(!controls.input)
        return;
    const int inset = controls.input->contentsRect().left() + controls.input->textMargins().left();
    for(QLabel *label : {controls.answerSourceLabel, controls.answerLabel})
        if(label && !label->text().isEmpty()) // 图标答案不补内缩：它按列位与上方预览对齐，加内缩反而错位
            label->setContentsMargins(inset+1, 0, 0, 0);
}

// 对照行里的乱序选框（池行才有；非池行该位置是 answerLabel）
QComboBox *LearningDialog::answerComboAt(int row) const
{
    QHBoxLayout *belowBox = answerBelowBox(row);
    const int slot = answerSlotIndexOf(row);
    QLayoutItem *item = belowBox && belowBox->count() > slot ? belowBox->itemAt(slot) : nullptr;
    return item ? qobject_cast<QComboBox *>(item->widget()) : nullptr;
}

QLabel *LearningDialog::summaryCheckLabel() const
{
    QGridLayout *layout = answerGrid();
    QLayoutItem *item = layout ? layout->itemAtPosition(answerRowCount, ColumnCheck) : nullptr;
    return item ? qobject_cast<QLabel *>(item->widget()) : nullptr;
}

QLabel *LearningDialog::summaryGoodTimeLabel() const // 汇总行的良好时间标签（在 answerRowCount 行，不能经 answerRowAt 取）
{
    QGridLayout *layout = answerGrid();
    QLayoutItem *item = layout ? layout->itemAtPosition(answerRowCount, ColumnGoodTime) : nullptr;
    return item ? qobject_cast<QLabel *>(item->widget()) : nullptr;
}

void LearningDialog::setKnowledgeViewSync(bool findActive,
                                          const QString &subj, const QString &pred, const QString &obj,
                                          const QSet<int> &expandedPhenIds, const QString &currentDerivedAbs)
{
    syncFindActive = findActive;
    syncSubj = subj;
    syncPred = pred;
    syncObj = obj;
    syncExpandedPhenIds = expandedPhenIds;
    syncCurrentDerivedAbs = currentDerivedAbs;
}

void LearningDialog::set_items_table(QString filter)
{
    sessionElapsed = QTime::fromMSecsSinceStartOfDay(0);
    chainCardByQId.clear(); // 每次打开复习重建"含链卡题"判定（两次复习间可能编辑过答案）
    gateDrillView.clear();  // 演练现场快照随本次复习集重建（旧揭示/回填不跨会话）
    questionTableModel->setFilter(filter);
    reloadQuestionTable(); // 先取全量，供门控分流扫描
    buildGatePlan();       // 扫描链卡题：有子行的隐藏全部子行+锁展开、其余为直接复习的链卡题

    reloadKnowledgeTreeFromSync();

    if(isSpeedLearn)
        ui->comboBox->setCurrentIndex(1);
    clear_question_display();
    ui->tableView->sortByColumn(0, Qt::AscendingOrder);
    on_comboBox_currentTextChanged(ui->comboBox->currentText());
    reloadQuestionTable();

    refreshAnswerMasks();    // 按本次复习集合初始化遮罩
    refreshTotalTimeLabels();
    buildGateGroups(); // 建树后分组：题目表列全部复习行，用户自选组与行
}

// 只读知识树按主界面同步快照重建（对话框模态期间主界面不能改图谱，只需载入时同步一次）
void LearningDialog::reloadKnowledgeTreeFromSync()
{
    delete knowledgeRoot;
    ui->subjLineEdit->setText(syncSubj);
    ui->predLineEdit->setText(syncPred);
    ui->objLineEdit->setText(syncObj);
    if(syncFindActive)
        knowledgeRoot = fillFindObjsModel(knowledgeModel, syncSubj, syncPred, syncObj);
    else
        knowledgeRoot = fillKnowledgeModel(knowledgeModel);
    setupKnowledgeTreeView(ui->phenTreeView); // 模型 clear 重建后 section 重置，须重新隐藏 @ 列
    if(syncExpandedPhenIds.isEmpty())
        ui->phenTreeView->collapseAll();
    else
        restoreExpandedPhenIds(ui->phenTreeView, syncExpandedPhenIds);
    if(!syncCurrentDerivedAbs.isEmpty())
        selectKnowledgeRowByDerivedAbs(ui->phenTreeView, syncCurrentDerivedAbs); // 同选主界面当前行（展开祖先）
}

// 复习集总时长与剩余时长（剩余量在总计时器里递减）
void LearningDialog::refreshTotalTimeLabels()
{
    totalGoodTime = QTime::fromMSecsSinceStartOfDay(0);
    for(int row = 0; row < questionTableModel->rowCount(); ++row)
        totalGoodTime = totalGoodTime.addMSecs(questionSql->get_question_learn_time(
            questionTableModel->index(row, 0).data().toInt()).msecsSinceStartOfDay());
    totalGoodTime = totalGoodTime.addMSecs(totalGoodTime.msecsSinceStartOfDay() * kRemainingTimeFactor);
    totalRemainingTimeMsec = totalGoodTime.msecsSinceStartOfDay();
    ui->totalGoodTimeLabel->setText(totalGoodTime.toString(QStringLiteral("hh'h':mm'm':ss's'")));
    ui->remainingTimeLabel->setText(ToolFunctions::sessionTimeText(QTime::fromMSecsSinceStartOfDay(totalRemainingTimeMsec)));
}

void LearningDialog::reloadQuestionTable()
{
    questionTableModel->select();
    ToolFunctions::fetchAllRows(questionTableModel);
}

void LearningDialog::on_tableView_clicked(const QModelIndex &index)
{
    set_question(index.siblingAtColumn(0).data().toInt());
}

// 知识检索：与主界面同一 tag 视图实现（主词空=world、谓词空=has）；复习窗口只读，模型重建后恢复遮罩与演练现场
void LearningDialog::on_findObjsPushButton_clicked()
{
    delete knowledgeRoot;
    knowledgeRoot = fillFindObjsModel(knowledgeModel,
                                      ui->subjLineEdit->text(), ui->predLineEdit->text(),
                                      ui->objLineEdit->text());
    setupKnowledgeTreeView(ui->phenTreeView); // 检索会 clear 重建模型，须重新隐藏 @ 列并恢复表头策略
    ui->phenTreeView->collapseAll();
    refreshAnswerMasks();   // 模型重建后按当前复习集合重新遮罩
    reapplyGateDrillView(); // 演练现场（回填/红绿揭示）按快照恢复
}

// 知识树行点击：按 DerivedAbs 在题目表中定位并跳转；未关联题目（或不在复习集内）的概念行不打断当前复习
void LearningDialog::on_phenTreeView_clicked(const QModelIndex &index)
{
    const QString derivedAbs = questionDerivedAbsForIndex(ui->phenTreeView, index); // 折叠态 ⇒ 合并行回退 is 子行；展开态点父行按父行 DerivedAbs 查题
    if(derivedAbs.isEmpty())
        return;
    const int row = ToolFunctions::rowOfValue(questionTableModel, derivedAbs, QuestionSql::headerColumnMap[kAbstractNameField]);
    if(row < 0)
        return;
    const int questionId = questionTableModel->index(row, 0).data().toInt();
    if(questionId == currentQuestionId)
        return; // 已在本题，避免重复加载
    ui->tableView->selectRow(row);
    set_question(questionId);
}

// 排序方式切换：重写 orderNum 后按 orderNum 列排序；两种排序模式共用收尾
void LearningDialog::on_comboBox_currentTextChanged(const QString &sortMode)
{
    stop_timer();
    const int rowCount = questionTableModel->rowCount();
    const bool randomOrder = sortMode == kRandomSortMode;
    const int orderGap = randomOrder || rowCount == 0 ? 0 : kMaxOrderNum / rowCount;
    for(int row = 0; row < rowCount; ++row)
    {
        const int questionId = questionTableModel->index(row, 0).data().toInt();
        const int orderNum = randomOrder ? QRandomGenerator::global()->bounded(kRandomOrderMax) : row * orderGap;
        questionSql->set_value(kQuestionsTable, questionId, kOrderNumField, orderNum);
    }
    if(randomOrder || sortMode == kDefaultSortMode)
    {
        ui->tableView->setSortingEnabled(true);
        ui->tableView->sortByColumn(QuestionSql::headerColumnMap[kOrderNumField], Qt::AscendingOrder);
    }
    ui->tableView->clearSelection();
    clear_question_display();
}

void LearningDialog::on_LearningDialog_finished(int)
{
    discardGateCollection(); // 未完成的本轮名称演练作废：零写库、树行恢复 "?"
    stop_timer();
    totalTimer->stop();
    answerStage = AnswerStage::Submitted; // 对话框已结束：后续不再进入答题分支
    lastReviewedQuestionId = currentQuestionId;
    currentQuestionId = -1;
    if(questionTableModel->rowCount() != 0)
        sessionElapsed = QTime::fromString(QStringLiteral("23:59:59.99"));
    ToolFunctions::watch_typst_stop(typstWatchProcess);
}