#include "learningdialog.h"
#include "ui_learningdialog.h"
#include "learningdialog_internal.h"
#include "utils/toolfunctions.h"
#include "core/fsrs.h"
#include "core/knowledgedb.h"
#include "widgets/knowledgetreehelper.h"
#include "widgets/questiontableviewhelper.h"
#include "services/questionpreview.h"
#include "services/typstimagecache.h"
#include <QSet>
#include <QStandardItemModel>
#include <QStandardItem>
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
const QString kDateStorageFormat = QStringLiteral("yyyy/MM/dd");

constexpr int kLineNumberWidth = 20;
constexpr int kDefaultInputPixelWidth = 10;
constexpr int kSummaryTimeWidth = 80;
const QString kManualInputStyle = QStringLiteral("QLineEdit {border: 1px solid blue;}");
const QString kImageInputStyle = QStringLiteral("QLineEdit {border: 1px solid red;}");
const QString kTypstInputStyle = QStringLiteral("QLineEdit {border: 1px solid green;}");
const QString kChainInputStyle = QStringLiteral("QLineEdit {border: 1px solid orange;}");

const QString kClozesTable = QStringLiteral("clozes");
const QString kNextDateField = QStringLiteral("nextDate");
const QString kInjectField = QStringLiteral("inject");

constexpr int kSpacerColumnSpan = 6; // 网格共 6 列（ColumnLineNumber..ColumnRating）
constexpr int kCompareBoxSpacing = 1; // 输入行与对照行的行距：贴合但不重叠

// 递归销毁一个格位里的全部件：输入列是"输入行 + 对照行"的嵌套布局，浅层清理会漏掉里层控件
void deleteLayoutItemTree(QLayoutItem *item)
{
    if(!item)
        return;
    if(QLayout *childLayout = item->layout())
        while(QLayoutItem *grandChild = childLayout->takeAt(0))
            deleteLayoutItemTree(grandChild);
    else
        delete item->widget();
    delete item;
}
}

void LearningDialog::clear_question_display()
{
    gateActive = false; // 链卡作答态随网格一起作废（已答记录在 gateRecords 中保留至该组大提交）
    gateInput = nullptr;
    gateChainRow = -1;

    ui->textBrowser->setText(QString()); // 清空题面与答案网格
    QGridLayout *layout = answerGrid();
    while(QLayoutItem *child = layout->takeAt(0))
        deleteLayoutItemTree(child);
}

void LearningDialog::set_question(int questionId)
{
    resetForNewQuestion(questionId);
    selectQuestionDerivedAbsInTree(questionId);
    rebuildAnswerGrid();

    if(!totalTimer->isActive())
        totalTimer->start();
    start_timer();

    if(gateCardIds.contains(questionId) && !gateAnsweredIds.contains(questionId)) // 链卡行：进入直接作答态
        enterGateMode(questionId);
}

void LearningDialog::resetForNewQuestion(int questionId)
{
    gateActive = false; // 上一题的链卡作答态结束；已答记录保留在 gateRecords 中待该组大提交
    gateInput = nullptr;
    gateChainRow = kNoGateRow;
    gatePendingRollGroup = kNoGroup; // 换题即失效：待回车切换只对刚提交的那一次生效

    answerStage = AnswerStage::Waiting;
    currentQuestionId = questionId;
    currentLineEdit = nullptr;
    manualRatings.clear(); // 换题即作废本题的自评档位
    manualCheckBaseline.clear();
    ui->idLabel->setText(QStringLiteral("id:%1").arg(currentQuestionId));
}

// 知识索引区联动：展开祖先并选中该题所属概念行（未关联概念时不动选择）；
// 本窗主树里没有该行时（如主界面当时是检索结果视图），走反选兜底（与主界面同口径，见 docs/01 §3.4）
void LearningDialog::selectQuestionDerivedAbsInTree(int questionId)
{
    const QString derivedAbs = questionSql->get_value(kQuestionsTable, questionId, kAbstractNameField).toString();
    if(!selectKnowledgeRowByDerivedAbs(ui->phenTreeView, derivedAbs))
        showObjectWordTreeFor(derivedAbs);
    refreshAnswerMasks(); // 遮罩集合不变，仅确保当前题对应行处于遮罩
}

// 反选兜底：逻辑上等价于 ctrl+左键该行的顶层宾词行——填好本窗的宾词视图并在其中选中该现象行
void LearningDialog::showObjectWordTreeFor(const QString &derivedAbs)
{
    const QString word = topLevelObjectWordOfDerivedAbs(derivedAbs);
    if(word.isEmpty())
        return; // 挂载键已失效（找不到所属顶层宾词）：不动视图，避免乱跳
    delete phenObjKnowledgeRoot;
    phenObjKnowledgeRoot = fillObjectWordModel(phenObjModel, word);
    setupKnowledgeTreeView(ui->phenObjTreeView); // 模型重建后 section 重置，需重新隐藏 @ 列
    selectKnowledgeRowByDerivedAbs(ui->phenObjTreeView, derivedAbs);
}

QString LearningDialog::answerImagePath(int answerId, int questionId) const
{
    return QuestionPreview::answerImagePath(questionId, answerId);
}

// 重建答案网格：每行一道填空（乱序池行换成选框），末尾为汇总行；onlyToLearn 下未到期答案回填进题面
void LearningDialog::rebuildAnswerGrid()
{
    clear_question_display();
    ui->textBrowser->setHtml(questionSql->read_questionHTML(currentQuestionId));
    currentAnswers = answerStore->answers(currentQuestionId);

    answerRowCount = currentAnswers.count();
    const QString derivedAbs = questionSql->get_value(kQuestionsTable, currentQuestionId, kAbstractNameField).toString();
    AnswerGridContext context;
    for(int row = 0; row < answerRowCount; ++row)
    {
        AnswerRowControls controls = createAnswerRowWidgets(row);
        applyAnswerStyle(controls, row, currentAnswers[row].toObject(), derivedAbs);
        placeAnswerRow(controls, row, currentAnswers[row].toObject(), context);
    }
    replacePoolAnswerCells(currentAnswers);
    showNotToLearnAnswers(context); // 未到期但需显示的空（乱序池里被隐藏的、作为注入的）
    appendAnswerSummaryRow(currentAnswers);
    appendNotToLearnAnswersToQuestion(currentAnswers, context.notToLearnRows);
}

// 一行的公共控件（样式、答案文本与宽度由 applyAnswerStyle 决定，是否参与作答由 placeAnswerRow 决定）
LearningDialog::AnswerRowControls LearningDialog::createAnswerRowWidgets(int rowIndex)
{
    AnswerRowControls controls;
    controls.lineNumber = new QLabel(this);
    controls.inputBox = new QHBoxLayout();
    controls.typstPreview = new QLabel(this);
    controls.input = new QLineEdit(); // statusTip 记录初始宽度（自适应下限）
    controls.answerLabel = new QLabel(); // statusTip 记录 cloze id
    controls.answerSourceLabel = new QLabel(this); // typst 行专有：标准答案的原文（摆在输入框正下方）
    controls.checkBox = new QCheckBox(this);
    controls.timeLabel = new QLabel(this);
    controls.goodTimeLabel = new QLabel(this);

    controls.lineNumber->setText(QString::number(rowIndex + 1));
    controls.lineNumber->setFixedWidth(kLineNumberWidth);
    controls.inputBox->addWidget(controls.input, 0, Qt::AlignLeft);
    controls.inputBox->addWidget(controls.typstPreview, 0, Qt::AlignLeft);
    controls.inputBox->addStretch();
    controls.typstPreview->hide();
    controls.answerSourceLabel->hide(); // 只有 typst 行在 applyAnswerStyle 里显示
    controls.input->setToolTip(QString::number(rowIndex)); // 空位编号联动用行号
    controls.checkBox->setChecked(false);
    controls.timeLabel->setText(QStringLiteral("00:00.00"));
    connect(controls.input, &QLineEdit::textChanged, this, &LearningDialog::onAnswerInputChanged);
    controls.input->installEventFilter(this);
    controls.ratingBox = createManualRatingBox(rowIndex);
    return controls;
}

// 按答案类型设置输入框样式、答案标签展示与初始宽度；链卡行同时记录门控收集行
void LearningDialog::applyAnswerStyle(const AnswerRowControls &controls, int rowIndex, const QJsonObject &answer, const QString &derivedAbs)
{
    const QString type = answer.value(AnswerStore::FieldType).toString();
    const QString content = answer.value(AnswerStore::FieldContent).toString();
    const int answerId = answer.value(AnswerStore::FieldId).toInt();
    controls.answerLabel->setStatusTip(QString::number(answerId));
    controls.goodTimeLabel->setText(ToolFunctions::ms2msz(
        questionSql->get_value(kClozesTable, answerId, kGoodTimeField).toString()));
    int inputWidth = kDefaultInputPixelWidth;
    if(type == AnswerStore::TypeAuto)
    {
        controls.answerLabel->setText(content);
        inputWidth = fontMetrics->horizontalAdvance(content);
    } else if(type == AnswerStore::TypeManual)
    {
        controls.input->setStyleSheet(kManualInputStyle);
        controls.answerLabel->setText(content);
        inputWidth = fontMetrics->horizontalAdvance(content);
    } else if(type == AnswerStore::TypeManualImage)
    {
        controls.input->setStyleSheet(kImageInputStyle);
        controls.answerLabel->setPixmap(QPixmap{content});
    } else if(type == AnswerStore::TypeAutoTypst)
    {
        controls.input->setStyleSheet(kTypstInputStyle);
        controls.answerLabel->setPixmap(QPixmap{QuestionPreview::answerImagePath(currentQuestionId, answerId)});
        controls.answerLabel->setToolTip(content);
        controls.answerLabel->setToolTipDuration(INT_MAX);
        // 原文摆在输入框正下方：与自己的输入逐字对照；渲染答案图则落到实时预览下方
        controls.answerSourceLabel->setText(content);
        controls.answerSourceLabel->show();
        controls.typstPreview->show();
        inputWidth = fontMetrics->horizontalAdvance(content);
    } else if(type == AnswerStore::TypeAutoChain)
    {
        controls.input->setStyleSheet(kChainInputStyle);
        const QString correctName = KnowledgeDB::lastObjectOfDerivedAbs(derivedAbs); // 标准答案 = 挂载 DerivedAbs 的末段宾词（图谱位置裁决，content 不用）；名字含 [ ] 也合法（回填时转义）
        controls.answerLabel->setText(correctName);
        inputWidth = fontMetrics->horizontalAdvance(correctName);
        gateChainRow = rowIndex; // 门控演练时该行输入框作收集器
    } else if(type == AnswerStore::TypeAutoChainTypst)
    {
        // 链卡(typst)：判分同链卡，但输入按 typst 公式对待——用户只写公式体（两端 $ 由类型承担），
        // 故用 typst 输入样式（绿框）驱动实时编译预览，宽度/提示都按公式体算；
        // 标准答案（图谱末段宾词 `$ … $`）渲染成公式图，与树内现象行观感一致
        controls.input->setStyleSheet(kTypstInputStyle);
        controls.typstPreview->show();
        const QString objectText = KnowledgeDB::lastObjectOfDerivedAbs(derivedAbs);
        const QString body = KnowledgeDB::chainTypstBodyOf(objectText); // 用户实际要输入的形式（剥两端 $）
        applyChainTypstAnswerLabel(controls.answerLabel, objectText);
        controls.answerSourceLabel->setText(body); // 原文 = 公式体，摆在输入框正下方
        controls.answerSourceLabel->show();
        inputWidth = fontMetrics->horizontalAdvance(body);
        gateChainRow = rowIndex;
    }
    controls.input->setStatusTip(QString::number(inputWidth + kInputWidthPadding));
    controls.input->setFixedWidth(inputWidth + kInputWidthPadding);
}

// 链卡(typst)标准答案：`$ … $` 交给 typst 公共缓存编译成图（悬停/回退显示公式体——即用户要输入的形式）；
// 图未就绪（首帧/typst 不可用）时先显示公式体，落盘后由 refreshChainTypstAnswerLabels 自动换成图
void LearningDialog::applyChainTypstAnswerLabel(QLabel *label, const QString &objectText)
{
    if(!label || objectText.isEmpty())
        return;
    const QString body = KnowledgeDB::chainTypstBodyOf(objectText);
    label->setToolTip(body);
    label->setToolTipDuration(INT_MAX);
    const int ppi = TypstImageCache::ppiOf(label);
    // 非 `$ … $` 完全包裹的宾词名不渲染，直接显示原文本（暂不支持混排，见 docs/01 约定 4.5a）
    const QPixmap image = KnowledgeDB::isChainTypstName(objectText)
                              ? TypstImageCache::instance()->pixmap(objectText, TypstImageCache::pointSizeOf(label->font()), ppi)
                              : QPixmap();
    if(image.isNull())
    {
        label->setText(body);
        return;
    }
    QPixmap shown = image;
    shown.setDevicePixelRatio(ppi / 96.0); // 编译图按 ppi 超采样：标 dpr 后 Qt 按逻辑尺寸绘制
    label->setText(QString());             // 先清文本，避免 setPixmap 与旧文本并存
    label->setContentsMargins(0, 0, 0, 0); // 由文本换成图后撤掉文字内缩：图要按列位与上方预览对齐
    label->setPixmap(shown);
}

// typst 图异步落盘后触发：把本题链卡(typst)行的标准答案从文本换成公式图
void LearningDialog::refreshChainTypstAnswerLabels()
{
    if(currentQuestionId < 0 || currentAnswers.isEmpty())
        return;
    const QString derivedAbs = questionSql->get_value(kQuestionsTable, currentQuestionId, kAbstractNameField).toString();
    if(derivedAbs.isEmpty())
        return;
    const QString objectText = KnowledgeDB::lastObjectOfDerivedAbs(derivedAbs);
    for(int row = 0; row < currentAnswers.size(); ++row)
        if(currentAnswers[row].toObject().value(AnswerStore::FieldType).toString() == AnswerStore::TypeAutoChainTypst)
            applyChainTypstAnswerLabel(answerRowAt(row).answerLabel, objectText);
}

// 放入网格：未到期的空只占一行显示答案文本（不参与作答），其余行装配输入框/勾选框/计时
void LearningDialog::placeAnswerRow(const AnswerRowControls &controls, int rowIndex, const QJsonObject &answer, AnswerGridContext &context)
{
    QGridLayout *layout = answerGrid();
    layout->addWidget(controls.lineNumber, rowIndex, ColumnLineNumber);

    const int answerId = answer.value(AnswerStore::FieldId).toInt();
    const QDate nextDate = QDate::fromString(questionSql->get_value(kClozesTable, answerId, kNextDateField).toString(),
                                             kDateStorageFormat);
    if(onlyToLearn && nextDate > QDate::currentDate()) // 未到期：隐藏输入与勾选，只留答案文本
    {
        layout->addWidget(controls.answerLabel, rowIndex, ColumnInput);
        controls.lineNumber->hide();
        controls.answerLabel->hide();
        context.notToLearnRows.append(rowIndex);
        return;
    }

    // 参与作答：纵向容器 = 输入行在上、对照行在正下方（上下齐平，便于逐字/逐图对照纠错）
    auto *compareBox = new QVBoxLayout();
    compareBox->setSpacing(kCompareBoxSpacing);
    compareBox->addLayout(controls.inputBox);
    auto *belowBox = new QHBoxLayout();
    if(!controls.answerSourceLabel->isHidden()) // typst 行：原文占"输入框正下方"位，答案图随之落到预览下方
    {
        controls.answerSourceLabel->setFixedWidth(controls.input->minimumWidth()); // 与输入框同宽 ⇒ 上下两列的第二个件（预览/答案图）左右对位
        belowBox->addWidget(controls.answerSourceLabel, 0, Qt::AlignLeft);
    }
    belowBox->addWidget(controls.answerLabel, 0, Qt::AlignLeft);
    belowBox->addStretch();
    compareBox->addLayout(belowBox);
    layout->addLayout(compareBox, rowIndex, ColumnInput);
    layout->addWidget(controls.checkBox, rowIndex, ColumnCheck);
    layout->addWidget(controls.timeLabel, rowIndex, ColumnTime);
    layout->addWidget(controls.goodTimeLabel, rowIndex, ColumnGoodTime);
    layout->addWidget(controls.ratingBox, rowIndex, ColumnRating); // 默认隐藏：揭示后按"本行占用的选项类型"决定是否给四档自评
    controls.answerSourceLabel->hide(); // 与答案一同揭示（未答完不泄露）
    controls.answerLabel->hide();
    controls.checkBox->hide();

    const int pool = answer.value(AnswerStore::FieldPool).toInt();
    if(pool != 0)
        context.toLearnPools.insert(pool);
    context.toLearnInjects.insert(questionSql->get_value(kClozesTable, answerId, kInjectField).toInt());
    if(currentLineEdit == nullptr)
        currentLineEdit = controls.input;
}

// 未到期但需要显示的空：它所在的乱序池在复习中，或它是注入答案 → 显示行号与答案文本
void LearningDialog::showNotToLearnAnswers(const AnswerGridContext &context)
{
    QGridLayout *layout = answerGrid();
    for(int row = 0; row < answerRowCount; ++row)
    {
        if(rowParticipates(row)) // 参与作答的行不在此列
            continue;
        const QJsonObject answer = currentAnswers[row].toObject();
        const bool poolShown = context.toLearnPools.contains(answer.value(AnswerStore::FieldPool).toInt());
        const bool injected = context.toLearnInjects.contains(answer.value(AnswerStore::FieldId).toInt());
        if(!poolShown && !injected)
            continue;
        if(QLabel *lineNumber = answerRowAt(row).lineNumber)
            lineNumber->show();
        if(QLabel *answerLabel = qobject_cast<QLabel *>(layout->itemAtPosition(row, ColumnInput)->widget()))
            answerLabel->show();
    }
}

// 乱序池行：把对照行里的答案标签换成下拉选框（选项 = 同池全部答案）
void LearningDialog::replacePoolAnswerCells(const QJsonArray &answers)
{
    for(int row = 0; row < answers.count(); ++row)
    {
        if(!rowParticipates(row)) // 未到期的空不参与作答
            continue;
        const QJsonObject answer = answers[row].toObject();
        const int pool = answer.value(AnswerStore::FieldPool).toInt();
        if(pool == 0)
            continue;
        QComboBox *comboBox = new QComboBox(this);
        comboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        comboBox->setIconSize(kIconLookupSize);
        connect(comboBox, &QComboBox::currentIndexChanged, this, &LearningDialog::onPoolAnswerChanged);
        for(int poolRow = 0; poolRow < answers.count(); ++poolRow)
        {
            if(!rowParticipates(poolRow))
                continue;
            const QJsonObject poolAnswer = answers[poolRow].toObject();
            if(poolAnswer.value(AnswerStore::FieldPool).toInt() == pool)
                addPoolComboBoxItem(comboBox, poolAnswer, poolRow);
        }
        comboBox->setToolTip(answer.value(AnswerStore::FieldContent).toString());
        comboBox->setCurrentIndex(comboBox->findData(row));
        QHBoxLayout *belowBox = answerBelowBox(row); // 选框顶掉对照行里的答案标签，位置随之落在输入框正下方
        const int slot = answerSlotIndexOf(row);     // 与 answerComboAt 同口径：typst 行下标 1，其余行下标 0
        QLayoutItem *answerItem = belowBox && belowBox->count() > slot ? belowBox->itemAt(slot) : nullptr;
        if(answerItem && answerItem->widget())
        {
            QWidget *replaced = answerItem->widget();
            belowBox->removeWidget(replaced); // removeWidget 会解除父子关系：须显式销毁，否则换题重建时泄漏
            delete replaced;
        }
        belowBox->insertWidget(slot, comboBox);
        comboBox->hide();
    }
}

// 池选项：图片/公式用图标，其余用文本；公式项另存 tooltip 供对照
void LearningDialog::addPoolComboBoxItem(QComboBox *comboBox, const QJsonObject &poolAnswer, int poolRow)
{
    const QString type = poolAnswer.value(AnswerStore::FieldType).toString();
    const QString content = poolAnswer.value(AnswerStore::FieldContent).toString();
    if(type == AnswerStore::TypeManualImage)
    {
        comboBox->addItem(QIcon{content}, QString(), poolRow);
    } else if(type == AnswerStore::TypeAutoTypst)
    {
        comboBox->addItem(QIcon{QuestionPreview::answerImagePath(currentQuestionId, poolAnswer.value(AnswerStore::FieldId).toInt())}, QString(), poolRow);
        comboBox->setItemData(comboBox->count() - 1, content, Qt::ToolTipRole);
    } else
    {
        comboBox->addItem(content, poolRow);
    }
}

// 底部汇总行：本题用时 / 良好时间 / 结论，并加一段底部留白
void LearningDialog::appendAnswerSummaryRow(const QJsonArray &answers)
{
    QGridLayout *layout = answerGrid();
    answerElapsedLabel = new QLabel(this);
    QLabel *goodTimeLabel = new QLabel(this);
    QLabel *ratingLabel = new QLabel(this);
    answerElapsedLabel->setText(QStringLiteral("00:00.00"));
    answerElapsedLabel->setFixedWidth(kSummaryTimeWidth);
    goodTimeLabel->setText(questionSql->get_question_learn_time(currentQuestionId).toString(kGoodTimeFormat));
    ratingLabel->setText(kRatingGoodText);
    layout->addWidget(answerElapsedLabel, answers.count(), ColumnTime);
    layout->addWidget(goodTimeLabel, answers.count(), ColumnGoodTime);
    layout->addWidget(ratingLabel, answers.count(), ColumnCheck);
    ratingLabel->hide();
    layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding),
                    answers.count() + 1, 0, 1, kSpacerColumnSpan);
}

// onlyToLearn：未到期的答案（含链卡名）以绿色回填题面，未到期的空不再是待答空
void LearningDialog::appendNotToLearnAnswersToQuestion(const QJsonArray &answers, const QVector<int> &notToLearnRows)
{
    if(!onlyToLearn || notToLearnRows.isEmpty())
        return;
    const QString derivedAbs = questionSql->get_value(kQuestionsTable, currentQuestionId, kAbstractNameField).toString();
    QTextDocument *document = ui->textBrowser->document();
    QTextCursor cursor(document);
    for(int row : notToLearnRows)
        for(QTextCursor &blankCursor : ToolFunctions::find_blanks_by_number(row + 1, cursor)) // 填空编号从 1 开始
            QuestionPreview::insertAnswerAtBlank(blankCursor, answers[row].toObject(), derivedAbs, document, currentQuestionId);
}