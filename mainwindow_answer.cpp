#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mainwindow_internal.h"

#include "parser/noteparser2.h"
#include "core/knowledgedb.h"
#include "widgets/knowledgetreehelper.h"
#include "widgets/questiontableviewhelper.h"
#include "dialogs/phenadddialog.h"
#include "services/typstimagecache.h"
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
#include <QPixmap>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QSize>
#include <QSqlDatabase>
#include <QSqlError>
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
const QString kTypstImagePathTemplate = QStringLiteral("./data/%1/%2.png");
const QSize kAnswerIconSize{1000, 1000};
const QString kDefaultAnswerText = QStringLiteral("请输入答案");
const QString kUnlinkedConceptText = QStringLiteral("（未关联概念）");
constexpr double kGoodTimeBaseSeconds = 3.5; // 前置读题时间
constexpr double kGoodTimePerLatinChar = 0.6;
constexpr double kGoodTimePerSymbolChar = 1.4;
constexpr double kGoodTimePerOtherChar = 0.8;
const char kLatinCharPattern[] = "[a-z0-9]";
const char kSymbolCharPattern[] = R"([<=>\-\+,\|()\{\}\[\]"/⟂∥±∪∩⟨⟩⋅∈∉×÷⇌≈⇒→←])";
} // namespace

// answer
void MainWindow::load_answer(int questionId)
{
    const QJsonArray answers = answerStore->answers(questionId);
    ui->answerTreeWidget->clear();
    ui->answerTreeWidget->setIconSize(kAnswerIconSize);
    for(int row = 0; row < answers.count(); row++)
    {
        const QJsonObject answer = answers[row].toObject();
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->answerTreeWidget); // id:StatusTip, type:ToolTip, content:WhatsThis
        item->setText(0, QString::number(row + 1));
        applyAnswerTypeDisplay(item, questionId,
                               answer[AnswerStore::FieldType].toString(),
                               answer[AnswerStore::FieldId].toInt(),
                               answer[AnswerStore::FieldContent].toString());
        item->setText(2, QString::number(answer[AnswerStore::FieldPool].toInt()));
        ui->answerTreeWidget->addTopLevelItem(item);
    }
}

// 第 1 列：图片/typst 用图标，链卡显示所挂 DerivedAbs 末段名（链卡(typst) 再把公式渲染成图）
void MainWindow::applyAnswerTypeDisplay(QTreeWidgetItem *item, int questionId, const QString &type, int answerId, const QString &content)
{
    if(type == AnswerStore::TypeManualImage)
    {
        item->setIcon(1, QIcon{content});
        item->setText(1, "");
    } else if(type == AnswerStore::TypeAutoTypst)
    {
        item->setIcon(1, QIcon{kTypstImagePathTemplate.arg(questionId).arg(answerId)});
        item->setText(1, "");
    } else if(AnswerStore::isChainCardType(type))
    {
        const QString derivedAbs = questionSql->get_value(kQuestionsTable, questionId, kAbstractNameField).toString();
        const QString objectText = derivedAbs.isEmpty() ? kUnlinkedConceptText : KnowledgeDB::lastObjectOfDerivedAbs(derivedAbs);
        const QString body = KnowledgeDB::chainTypstBodyOf(objectText); // 内容文本去掉两端 $：用户复习时输入的就是这个形式
        item->setToolTip(1, body);
        if(type == AnswerStore::TypeAutoChain || !KnowledgeDB::isChainTypstName(objectText))
        {
            item->setText(1, type == AnswerStore::TypeAutoChain ? objectText : body);
            return;
        }
        const int ppi = TypstImageCache::ppiOf(ui->answerTreeWidget);
        const QPixmap image = TypstImageCache::instance()->pixmap(objectText, TypstImageCache::pointSizeOf(ui->answerTreeWidget->font()), ppi);
        if(image.isNull()) // 图未就绪（首次遇到/typst 不可用）：先显示公式体，落盘后由 refreshChainTypstAnswerIcons 换成图
        {
            item->setIcon(1, QIcon());
            item->setText(1, body);
            return;
        }
        // 本列表的图标按像素原样铺开（iconSize 只是上限），故先缩到"逻辑尺寸 × 屏幕缩放"，
        // 否则编译用的 2x 超采样会让公式图比同排文字大一倍
        const QSize shown = TypstImageCache::displaySizeOf(image, ppi) * ui->answerTreeWidget->devicePixelRatioF();
        item->setIcon(1, QIcon{image.scaled(shown, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)});
        item->setText(1, "");
    } else
    {
        item->setText(1, content);
    }
}

// typst 图异步落盘后触发：当前题的链卡(typst)答案从公式体文本换成渲染图
void MainWindow::refreshChainTypstAnswerIcons()
{
    if(currentQuestionId < 0)
        return;
    const QJsonArray answers = answerStore->answers(currentQuestionId);
    for(int row = 0; row < answers.count(); ++row)
    {
        const QJsonObject answer = answers[row].toObject();
        if(answer.value(AnswerStore::FieldType).toString() != AnswerStore::TypeAutoChainTypst)
            continue;
        if(QTreeWidgetItem *item = answerRowItem(row))
            applyAnswerTypeDisplay(item, currentQuestionId, AnswerStore::TypeAutoChainTypst,
                                   answer.value(AnswerStore::FieldId).toInt(), answer.value(AnswerStore::FieldContent).toString());
    }
}

void MainWindow::init_answerTableView()
{
    answerTableModel = new QSqlTableModel(this, questionSql->getDb());
    answerTableModel->setTable(kClozesTable);
    answerTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);

    ui->answerTableView->setModel(answerTableModel);
    ui->answerTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    setAnswerTableHeaders(answerTableModel);
}

void MainWindow::init_answerTreeWidget()
{
    ui->answerTreeWidget->setDragDropMode(QAbstractItemView::InternalMove);
    ui->answerTreeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->answerTreeWidget->headerItem()->setHidden(false);
    ui->answerTreeWidget->setIndentation(0);
    ui->answerTreeWidget->setHeaderLabels({"num", "content", "pool"});
    ui->answerTreeWidget->header()->setStretchLastSection(false);
    ui->answerTreeWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->answerTreeWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);

    connect(ui->answerTreeWidget, &DragQTreeWidget::signalRowsMoved,
            this, &MainWindow::answerItemRowsMoved);
    // 链卡(typst)的答案图是异步编译的：落盘后把公式体文本换成图
    connect(TypstImageCache::instance(), &TypstImageCache::imageReady, this, &MainWindow::refreshChainTypstAnswerIcons);
}

QTreeWidgetItem *MainWindow::answerRowItem(int row) const
{
    const auto items = ui->answerTreeWidget->findItems(QString::number(row + 1), Qt::MatchExactly, 0);
    return items.isEmpty() ? nullptr : items.first();
}

void MainWindow::reloadAnswersSelectingNeighbor(int questionId, int row)
{
    load_answer(questionId);
    const int nextRow = qMin(row, ui->answerTreeWidget->topLevelItemCount() - 1);
    ui->answerTreeWidget->setCurrentItem(answerRowItem(nextRow));
}

void MainWindow::openAnswerEditDialog(int questionId, int oldAnswerId, const QJsonObject &answer, const QJsonArray &siblings)
{
    answerEditDialog->setQId(questionId);
    answerEditDialog->resetEdit();
    answerEditDialog->setGoodTime(ToolFunctions::ms2QTime(questionSql->get_value(kClozesTable, oldAnswerId, kGoodTimeField).toString()));
    answerEditDialog->setAId(oldAnswerId);
    answerEditDialog->setType(answer[AnswerStore::FieldType].toString());
    answerEditDialog->setPool(answer[AnswerStore::FieldPool].toInt());
    answerEditDialog->setContent(answer[AnswerStore::FieldContent].toString());
    answerEditDialog->lineEdit_selectAll();
    answerEditDialog->setSiblingAnswers(siblings); // 链卡同题唯一校验用
    answerEditDialog->setInjectBId(questionSql->get_value(kClozesTable, oldAnswerId, kInjectField).toInt());
}

// aid 改绑时同步引用计数；第 1/3 列显示随之刷新
void MainWindow::applyAnswerEditResult(int questionId, int row, const QJsonObject &previous, QTreeWidgetItem *item)
{
    const QString type = answerEditDialog->getRetType();
    const QString content = AnswerStore::isChainCardType(type) ? QString()
                                                              : answerEditDialog->getRetContent(); // 链卡无手填答案，防御性清空
    const int answerId = answerEditDialog->getRetAId();
    const int pool = answerEditDialog->getRetPool();
    const QString goodTime = answerEditDialog->getRetGoodTime().toString(kGoodTimeFormat);

    QJsonObject updated = previous;
    updated[AnswerStore::FieldContent] = content;
    updated[AnswerStore::FieldDisplay] = "";
    updated[AnswerStore::FieldId] = answerId;
    updated[AnswerStore::FieldType] = type;
    updated[AnswerStore::FieldPool] = pool;
    answerStore->replaceAnswerAt(questionId, row, updated);
    questionSql->set_value(kClozesTable, answerId, kGoodTimeField, goodTime);
    questionSql->set_value(kClozesTable, answerId, kInjectField, answerEditDialog->getRetInjectBId());
    questionSql->update_question_state(questionId);

    applyAnswerTypeDisplay(item, questionId, type, answerId, content);
    item->setText(2, QString::number(pool));
}

void MainWindow::on_answerTreeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if(!ui->questionTableView->currentIndex().isValid() || item == nullptr)
        return;

    const int questionId = currentQuestionId;
    const int row = rowOfAnswerItem(item);
    const QJsonArray answers = answerStore->answers(questionId);
    if(row < 0 || row >= answers.count())
        return;

    const QJsonObject answer = answers[row].toObject();
    const int oldAnswerId = answer[AnswerStore::FieldId].toInt();
    openAnswerEditDialog(questionId, oldAnswerId, answer, answers);
    if(!answerEditDialog->exec())
        return;
    applyAnswerEditResult(questionId, row, answer, item);
}

void MainWindow::on_answerTreeWidget_itemActivated(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    QString filter;
    if(item != nullptr)
    {
        const int answerId = answerStore->answer(currentQuestionId, rowOfAnswerItem(item)).value(AnswerStore::FieldId).toInt();
        filter = QString("id = %1").arg(answerId);
    }
    answerTableModel->setFilter(filter);
    answerTableModel->select();
    ui->answerTableView->selectRow(0);
}

void MainWindow::on_answerTreeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous);
    on_answerTreeWidget_itemClicked(current, 0);
}

void MainWindow::on_answerTreeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    if(item == nullptr)
        return;
    const int selectedAnswerId = ui->answerTableView->currentIndex().siblingAtColumn(0).data().toInt();
    const int rowAnswerId = answerStore->answer(currentQuestionId, rowOfAnswerItem(item)).value(AnswerStore::FieldId).toInt();
    if(selectedAnswerId != rowAnswerId)
        on_answerTreeWidget_itemActivated(item, column);
}

void MainWindow::on_answerTreeWidget_itemChanged(QTreeWidgetItem *item, int column)
{
    on_answerTreeWidget_itemClicked(item, column);
}

void MainWindow::answerItemRowsMoved(int startIndex, int endIndex)
{
    if(!ui->questionTableView->currentIndex().isValid())
        return;

    const int questionId = currentQuestionId;
    answerStore->moveAnswerWithinQuestion(questionId, startIndex, endIndex);
    load_answer(questionId);
    const int movedRow = qMin(endIndex, ui->answerTreeWidget->topLevelItemCount() - 1);
    QTreeWidgetItem *movedItem = answerRowItem(movedRow);
    if(movedItem != nullptr)
        ui->answerTreeWidget->setCurrentItem(movedItem);
}

void MainWindow::on_answerAddButton_clicked()
{
    if(!ui->questionTableView->currentIndex().isValid())
        return;

    const int questionId = currentQuestionId;
    const int row = ui->answerTreeWidget->currentIndex().row() + 1;
    QString selectedText = ui->questionTextEdit->textCursor().selectedText();
    if(selectedText.isEmpty())
        selectedText = kDefaultAnswerText;
    answerStore->insertAnswer(questionId, row, selectedText);

    load_answer(questionId);
    QTreeWidgetItem *newItem = answerRowItem(row);
    if(newItem == nullptr)
        return;
    ui->answerTreeWidget->setCurrentItem(newItem);
    on_answerTreeWidget_itemDoubleClicked(newItem, 0); // 立即进入编辑
}

void MainWindow::on_answerDelButton_clicked()
{
    if(!ui->questionTableView->currentIndex().isValid())
        return;
    QTreeWidgetItem *item = ui->answerTreeWidget->currentItem();
    if(item == nullptr)
        return;

    const int questionId = currentQuestionId;
    const int row = rowOfAnswerItem(item);
    answerStore->removeAnswerAt(questionId, row);
    reloadAnswersSelectingNeighbor(questionId, row);
}

// 校验并返回移动源：源题目 id 与答案行号（row 出参）；任一步不合法返回 -1
int MainWindow::takeClozeMoveSource(int &row)
{
    if(!ui->questionTableView->currentIndex().isValid() || currentQuestionId <= 0)
    {
        QMessageBox::information(this, QStringLiteral("移动答案"), QStringLiteral("请先在题目表中选择源题目。"));
        return -1;
    }
    QTreeWidgetItem *item = ui->answerTreeWidget->currentItem();
    if(item == nullptr)
    {
        QMessageBox::information(this, QStringLiteral("移动答案"), QStringLiteral("请先在答案列表中选择要移动的答案。"));
        return -1;
    }

    const int sourceQuestionId = currentQuestionId;
    row = rowOfAnswerItem(item);
    if(row < 0 || row >= answerStore->answers(sourceQuestionId).count())
        return -1;
    return sourceQuestionId;
}

void MainWindow::on_clozeMovePushButton_clicked()
{
    int row = -1;
    const int sourceQuestionId = takeClozeMoveSource(row);
    if(sourceQuestionId < 0)
        return;

    const QString sourceDerivedAbs = questionSql->get_value(kQuestionsTable, sourceQuestionId, kAbstractNameField).toString();
    clozeMoveDialog->setup(sourceDerivedAbs, collectExpandedPhenIds(), sourceQuestionId);
    if(clozeMoveDialog->exec() != QDialog::Accepted)
        return;
    const int targetQuestionId = clozeMoveDialog->selectedQId();
    if(targetQuestionId <= 0 || targetQuestionId == sourceQuestionId)
        return;

    QString error;
    if(!answerStore->moveAnswerToQuestion(sourceQuestionId, row, targetQuestionId, &error))
    {
        QMessageBox::warning(this, QStringLiteral("移动答案"), error);
        return;
    }

    reloadAnswersSelectingNeighbor(sourceQuestionId, row);
    questionTableModel->select();
    refreshAllDueDaysColumns(); // 答案迁移改了两题的到期聚合：知识树天数随之刷新
}

// 均分给每条 cloze 再乘回总数写回题目，保证题目值=各空之和
void MainWindow::distributeGoodTime(int questionId, QTime &time)
{
    const QJsonArray answers = answerStore->answers(questionId);
    time = QTime::fromMSecsSinceStartOfDay(time.msecsSinceStartOfDay() / answers.count() / 1000 * 1000);
    for(const QJsonValue &answer : answers)
    {
        const int answerId = answer.toObject().value(AnswerStore::FieldId).toInt();
        questionSql->set_value(kClozesTable, answerId, kGoodTimeField, time.toString(kGoodTimeFormat));
    }
    time = QTime::fromMSecsSinceStartOfDay(time.msecsSinceStartOfDay() * answers.count());
    questionSql->set_value(kQuestionsTable, questionId, kGoodTimeField, time.toString(kGoodTimeFormat));
}

void MainWindow::on_setGoodTimeButton_clicked()
{
    const QModelIndexList indexes = ui->questionTableView->selectionModel()->selectedIndexes();
    if(indexes.count() <= 0)
        return;
    const int firstQuestionId = indexes.begin()->siblingAtColumn(QuestionIdColumn).data().toInt();
    const QTime goodTime = QTime::fromString(questionSql->get_value(kQuestionsTable, firstQuestionId, kGoodTimeField).toString(), kGoodTimeFormat);
    setTimeDialog->initTime(goodTime);
    setTimeDialog->exec();
    QTime time = setTimeDialog->getTime();
    if(!time.isValid())
        return;

    for(const QModelIndex &index : indexes)
        distributeGoodTime(index.siblingAtColumn(QuestionIdColumn).data().toInt(), time);

    questionTableModel->select();
    const QModelIndex newIndex = *questionTableModel->match(questionTableModel->index(0, 0), Qt::DisplayRole, firstQuestionId).begin();
    ui->questionTableView->setCurrentIndex(newIndex);
    on_questionTableView_activated(newIndex);
}

// 一般适用于文章背诵场景
void MainWindow::on_questionAutoBindInjectButton_clicked()
{
    const QJsonArray answers = answerStore->answers(currentQuestionId);
    for(int i = 1; i < answers.count(); i++)
    {
        const int previousAnswerId = answers[i - 1].toObject()[AnswerStore::FieldId].toInt();
        const int currentAnswerId = answers[i].toObject()[AnswerStore::FieldId].toInt();
        questionSql->set_value(kClozesTable, currentAnswerId, kInjectField, previousAnswerId);
    }
}

// 按各类字符系数估算良好时间
void MainWindow::on_questionAutoGoodTimeButton_clicked()
{
    const QJsonArray answers = answerStore->answers(currentQuestionId);
    const QString derivedAbs = questionSql->get_value(kQuestionsTable, currentQuestionId, kAbstractNameField).toString();
    const QRegularExpression latinPattern(kLatinCharPattern);
    const QRegularExpression symbolPattern(kSymbolCharPattern);
    for(const QJsonValue &value : answers)
    {
        const QJsonObject answer = value.toObject();
        const int answerId = answer[AnswerStore::FieldId].toInt();
        const QString answerType = answer[AnswerStore::FieldType].toString();
        QString answerContent = answer[AnswerStore::FieldContent].toString();
        if(AnswerStore::isChainCardType(answerType))
        {
            // 链卡的标准答案不在 content 里（编辑时恒空），而由图谱挂载位置裁决——按用户实际要输入的形式估时，
            // 否则一律退化成只有前置读题时间（与判分/揭示口径不一致）
            const QString objectText = derivedAbs.isEmpty() ? QString() : KnowledgeDB::lastObjectOfDerivedAbs(derivedAbs);
            answerContent = answerType == AnswerStore::TypeAutoChainTypst
                                ? KnowledgeDB::chainTypstBodyOf(objectText) // 只敲公式体（两端 $ 由类型承担）
                                : objectText;
        }
        double goodTimeSec = kGoodTimeBaseSeconds;
        goodTimeSec += kGoodTimePerLatinChar * answerContent.count(latinPattern);
        goodTimeSec += kGoodTimePerSymbolChar * answerContent.count(symbolPattern);
        goodTimeSec += kGoodTimePerOtherChar * (answerContent.size() - answerContent.count(latinPattern) - answerContent.count(symbolPattern));
        const QTime answerGoodTime = QTime::fromMSecsSinceStartOfDay(qCeil(goodTimeSec) * 1000); // 向上取整8/14
        questionSql->set_value(kClozesTable, answerId, kGoodTimeField, answerGoodTime.toString(kGoodTimeFormat));
    }
    questionSql->update_question_state(currentQuestionId); // 题目聚合列（良好用时/平均用时/最佳用时）随之重算
    // 原生 SQL 写入模型不感知：重取题目表并回选当前行（同题不会重载题面），使「良好用时」列即时显示新值
    questionTableModel->select();
    ToolFunctions::fetchAllRows(questionTableModel);
    select_question_by_id(currentQuestionId);
    if(QTreeWidgetItem *item = ui->answerTreeWidget->currentItem()) // 当前空的排期表同样显示「良好用时」
        on_answerTreeWidget_itemActivated(item, 0);
}

void MainWindow::on_questionResetButton_clicked()
{
    const QJsonArray answers = answerStore->answers(currentQuestionId);
    for(const QJsonValue &value : answers)
        questionSql->resetFSRSData(value.toObject()[AnswerStore::FieldId].toInt());
    // questions 的 FSRS 列是该题 cloze 的聚合冗余，重置后必须重算，否则题目仍按旧到期日显示、进不了待学列表
    questionSql->update_question_state(currentQuestionId);
    questionTableModel->select();
    select_question_by_id(currentQuestionId);
}

// 只改到期日使其立刻进入待学（cloze 级）
void MainWindow::on_clozeRecallTodayButton_clicked()
{
    QTreeWidgetItem *item = ui->answerTreeWidget->currentItem();
    if(currentQuestionId <= 0 || item == nullptr)
    {
        QMessageBox::information(this, QStringLiteral("提前复习"),
                                 QStringLiteral("请先在题目表选题目，并在答案列表选中要提前复习的空。"));
        return;
    }
    const int answerId = answerStore->answer(currentQuestionId, rowOfAnswerItem(item)).value(AnswerStore::FieldId).toInt();
    if(answerId <= 0)
        return;
    setClozeDueToday(answerId);
    questionSql->update_question_state(currentQuestionId); // 题目到期日取其空的最早值，可能随之提前
    refreshDueDatesAfterChange();
}

void MainWindow::on_questionRecallTodayButton_clicked()
{
    if(currentQuestionId <= 0)
    {
        QMessageBox::information(this, QStringLiteral("提前复习"), QStringLiteral("请先在题目表中选择一道题目。"));
        return;
    }
    const QJsonArray answers = answerStore->answers(currentQuestionId);
    if(answers.isEmpty())
        return;
    for(const QJsonValue &value : answers)
        setClozeDueToday(value.toObject()[AnswerStore::FieldId].toInt());
    questionSql->update_question_state(currentQuestionId);
    refreshDueDatesAfterChange();
}

// 表项批量提前复习：当前过滤后表里每题的每个空都改到今天（先警告确认，整体一个事务）
void MainWindow::on_questionTableRecallTodayButton_clicked()
{
    const int rowCount = questionTableModel->rowCount();
    if(rowCount <= 0)
    {
        QMessageBox::information(this, QStringLiteral("提前复习(表项)"),
                                 QStringLiteral("题目表当前没有题目（或被名称/概念范围过滤空了）。"));
        return;
    }

    const QMessageBox::StandardButton choice = QMessageBox::warning(
        this, QStringLiteral("提前复习(表项)"),
        QStringLiteral("即将把表内全部 %1 道题的所有空的到期日改成今天，它们会立即进入待学。\n"
                       "只改到期日，学习历史（状态/用时等）不变；此操作无法撤销。\n\n"
                       "确定要执行吗?").arg(rowCount),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if(choice != QMessageBox::Yes)
        return;

    QSqlDatabase database = questionSql->getDb();
    const bool transactionStarted = database.transaction(); // 全表改期必须原子：中途失败不能留下"半数题已提前"的节奏
    int changed = 0;
    for(int row = 0; row < rowCount; ++row)
    {
        const int questionId = questionTableModel->index(row, QuestionIdColumn).data().toInt();
        if(questionId <= 0)
            continue;
        const QJsonArray answers = answerStore->answers(questionId);
        if(answers.isEmpty())
            continue;
        for(const QJsonValue &value : answers)
            setClozeDueToday(value.toObject()[AnswerStore::FieldId].toInt());
        questionSql->update_question_state(questionId); // 题目到期日取其空的最早值
        ++changed;
    }
    if(transactionStarted && !database.commit())
    {
        database.rollback();
        QMessageBox::warning(this, QStringLiteral("提前复习(表项)"), database.lastError().text());
        return;
    }

    refreshDueDatesAfterChange();
    QMessageBox::information(this, QStringLiteral("提前复习(表项)"),
                             QStringLiteral("已把 %1 道题的全部空改为今天到期。").arg(changed));
}

// 只把到期日改成今天：state/lastS 等学习历史一律不动（"提前复习"≠"重置学习数据"）
void MainWindow::setClozeDueToday(int answerId)
{
    questionSql->set_value(kClozesTable, answerId, kNextDateField, QDate::currentDate().toString(kDateStorageFormat));
}

void MainWindow::refreshDueDatesAfterChange()
{
    questionTableModel->select();
    select_question_by_id(currentQuestionId); // 先重取再回选当前行（同题不会重载题面）
    refreshAllDueDaysColumns();
    if(QTreeWidgetItem *item = ui->answerTreeWidget->currentItem())
        on_answerTreeWidget_itemActivated(item, 0); // 当前空的排期表刷新（nextDate 列）
}
