#include "learningdialog.h"
#include "ui_learningdialog.h"
#include "learningdialog_internal.h"
#include "utils/toolfunctions.h"
#include "core/fsrs.h"
#include "core/knowledgedb.h"
#include "widgets/knowledgetreehelper.h"
#include "widgets/questiontableviewhelper.h"
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
// 评级文案
const QString kRatingHardText = QStringLiteral("困难");
const QString kRatingEasyText = QStringLiteral("简单");
const QString kRatingWrongText = QStringLiteral("出错");
const QString kRatingWrongKey = QStringLiteral("wrong"); // FSRS::rating 的键：强制评分写库用
const QString kRatingHardKey = QStringLiteral("hard");
const QString kRatingGoodKey = QStringLiteral("good");
const QString kRatingEasyKey = QStringLiteral("easy");
constexpr int kManualRatingSpacing = 2;
const char kRatingOptionIndexProperty[] = "ratingOptionIndex"; // 自评按钮上挂的档位下标
// manual 行四档自评（按钮文案 → FSRS 评分键），顺序按难度递减
const QList<QPair<QString, QString>> kManualRatingOptions = {
    {QStringLiteral("错误"), kRatingWrongKey},
    {QStringLiteral("困难"), kRatingHardKey},
    {QStringLiteral("良好"), kRatingGoodKey},
    {QStringLiteral("简单"), kRatingEasyKey}};

QString ratingTextOf(const QString &rating) // FSRS 评分键 → 界面文案
{
    if(rating == kRatingWrongKey)
        return kRatingWrongText;
    if(rating == kRatingHardKey)
        return kRatingHardText;
    if(rating == kRatingEasyKey)
        return kRatingEasyText;
    return kRatingGoodText;
}

const QString kGateLockedHint = QStringLiteral("已确认，再次点击「提交」完成本题");

// 能文本比对、可机器判分的答案类型（manual / manual(image) 只能由用户自判）
bool isAutoCheckedAnswer(const QString &type)
{
    return type == AnswerStore::TypeAuto || type == AnswerStore::TypeAutoTypst;
}

// 只在"自动检查"选项里按内容找匹配（未命中 -1）；手动选项由用户在下拉里自行指定，不参与匹配
int matchedPoolItem(const QComboBox *comboBox, const QString &input, const QJsonArray &answers)
{
    for(int index = 0; index < comboBox->count(); ++index)
    {
        const QJsonObject poolAnswer = answers[comboBox->itemData(index).toInt()].toObject();
        if(isAutoCheckedAnswer(poolAnswer.value(AnswerStore::FieldType).toString())
           && input == poolAnswer.value(AnswerStore::FieldContent).toString())
            return index;
    }
    return -1;
}

// 本行实际会记到哪个池选项上（判分方式跟随它，而不是本行自己的类型）
QJsonObject consumedPoolAnswer(const QComboBox *comboBox, const QJsonArray &answers)
{
    return answers[comboBox->currentData().toInt()].toObject();
}
}

// manual 行四档自评：选中即由档位接管对错（勾选框只读），再点同一档取消；故不排他、允许自取消
QWidget *LearningDialog::createManualRatingBox(int rowIndex)
{
    QWidget *box = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kManualRatingSpacing);
    for(int index = 0; index < kManualRatingOptions.count(); ++index)
    {
        QToolButton *button = new QToolButton(box);
        button->setText(kManualRatingOptions[index].first);
        button->setCheckable(true);
        button->setProperty(kRatingOptionIndexProperty, index); // 点击回调据此取回 FSRS 评分键
        layout->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, rowIndex, button]() {
            applyManualRating(rowIndex, button->property(kRatingOptionIndexProperty).toInt(), button->isChecked());
        });
    }
    box->hide();
    return box;
}

// 展示某行的四档自评，并记下"未自评时"的勾选框判定结果（取消自评要按它还原）
void LearningDialog::showManualRatingRow(int rowIndex)
{
    AnswerRowControls controls = answerRowAt(rowIndex);
    if(!controls.ratingBox || !controls.checkBox)
        return;
    manualCheckBaseline[rowIndex] = {controls.checkBox->isChecked(), controls.checkBox->isEnabled()};
    controls.ratingBox->show();
}

// 自评：选中 → 由档位接管对错并把勾选框置为只读（选"错误"即答错）；取消 → 恢复勾选框的自动判定结果
void LearningDialog::applyManualRating(int rowIndex, int optionIndex, bool selected)
{
    AnswerRowControls controls = answerRowAt(rowIndex);
    if(!controls.ratingBox || !controls.checkBox)
        return;
    if(!selected)
    {
        clearManualRating(rowIndex);
        return;
    }
    for(QToolButton *button : controls.ratingBox->findChildren<QToolButton *>()) // 同行的其他档位取消选中
        button->setChecked(button->property(kRatingOptionIndexProperty).toInt() == optionIndex);
    const QString rating = kManualRatingOptions[optionIndex].second;
    manualRatings[rowIndex] = rating;
    controls.checkBox->setChecked(rating != kRatingWrongKey);
    controls.checkBox->setEnabled(false); // 对错已由所选档位决定，禁止再手动改勾选框
}

// 取消自评：清档位与记档，并把勾选框还原成"未自评时"的判定结果与可编辑性
void LearningDialog::clearManualRating(int rowIndex)
{
    AnswerRowControls controls = answerRowAt(rowIndex);
    manualRatings.remove(rowIndex);
    if(controls.ratingBox)
        for(QToolButton *button : controls.ratingBox->findChildren<QToolButton *>())
            button->setChecked(false);
    if(!controls.checkBox)
        return;
    const QPair<bool, bool> baseline = manualCheckBaseline.value(rowIndex, {false, true});
    controls.checkBox->setChecked(baseline.first);
    controls.checkBox->setEnabled(baseline.second);
}

QString LearningDialog::chosenManualRating(int rowIndex) const
{
    return manualRatings.value(rowIndex);
}

QString LearningDialog::firstChosenManualRating() const
{
    for(int row = 0; row < answerRowCount; ++row)
        if(manualRatings.contains(row))
            return manualRatings.value(row);
    return QString();
}

void LearningDialog::preSubmit()
{
    answerStage = AnswerStage::PreSubmitted;

    if(gateActive) // 链卡行：与普通题一致只锁定输入区，但不揭示对错——归属判定与红绿揭示都在组大提交时进行
    {
        if(gateInput)
        {
            gateInput->setEnabled(false);
            gateInput->setPlaceholderText(kGateLockedHint);
        }
        return;
    }

    const QJsonArray answers = answerStore->answers(currentQuestionId);
    for(int row = 0; row < answerRowCount; ++row)
    {
        if(!rowParticipates(row)) // 未到期的空：显示行号与答案文本
        {
            AnswerRowControls rowControls = answerRowAt(row);
            if(rowControls.lineNumber)
                rowControls.lineNumber->show();
            if(rowControls.answerLabel)
                rowControls.answerLabel->show();
            continue;
        }
        revealAnswerRow(row, answers[row].toObject(), answers);
    }

    QTimer::singleShot(2, this, [this]() { // 延迟 2ms 等布局更新后再聚焦
        if(currentLineEdit != nullptr)
            ui->scrollArea->ensureWidgetVisible(currentLineEdit);
    });
}

// 预提交一行：锁定输入、显示答案；非乱序池行自动判分（manual 不匹配时仍交还自判）
void LearningDialog::revealAnswerRow(int row, const QJsonObject &answer, const QJsonArray &answers)
{
    AnswerRowControls controls = answerRowAt(row);
    if(!controls.input || !controls.checkBox)
        return;
    controls.input->setEnabled(false);
    controls.checkBox->show();

    if(answer.value(AnswerStore::FieldPool).toInt() != 0)
    {
        revealPoolAnswerRow(row, answer, answers);
        return;
    }
    checkAnswerRow(controls, answer);
    const QString type = answer.value(AnswerStore::FieldType).toString();
    if(controls.ratingBox && (type == AnswerStore::TypeManual || type == AnswerStore::TypeManualImage))
        showManualRatingRow(row); // manual 行揭示后给出四档自评（不点则沿用用时反推）
    const bool notStudiedYet = controls.timeLabel && ToolFunctions::isUnstudied(controls.timeLabel->text());
    alignCompareLabels(controls); // 此时几何已定：按输入框内容区左内缩对齐，文字/图形逐像素对位
    if(controls.answerLabel && !notStudiedYet) // 从未学过的空不揭示答案
        controls.answerLabel->show();
    if(controls.answerSourceLabel && !notStudiedYet) // typst 行的原文与答案一同揭示（对照纠错用）
        controls.answerSourceLabel->show();
}

// 非乱序池行判分：auto/manual 比文本、typst 比 tooltip、链卡按挂载的 DerivedAbs 重建（零写库）
void LearningDialog::checkAnswerRow(const AnswerRowControls &controls, const QJsonObject &answer)
{
    const QString type = answer.value(AnswerStore::FieldType).toString();
    const QString input = controls.input ? controls.input->text() : QString();
    if(type == AnswerStore::TypeAuto || type == AnswerStore::TypeManual)
    {
        controls.checkBox->setChecked(input == controls.answerLabel->text());
        controls.checkBox->setEnabled(false);
        if(type == AnswerStore::TypeManual && !controls.checkBox->isChecked())
            controls.checkBox->setEnabled(true); // 手动型不匹配：交还用户自判
    } else if(type == AnswerStore::TypeAutoTypst)
    {
        controls.checkBox->setChecked(input == controls.answerLabel->toolTip());
        controls.checkBox->setEnabled(false);
    } else if(type == AnswerStore::TypeAutoChain)
    {
        const QString derivedAbs = questionSql->get_value(kQuestionsTable, currentQuestionId, kAbstractNameField).toString();
        const QString rebuilt = KnowledgeDB::replaceLastObjectOfDerivedAbs(derivedAbs, input); // 输入替换末段宾词重建该名，重建名 == 挂载名才判对（等价于答出末段概念名）
        controls.checkBox->setChecked(!derivedAbs.isEmpty() && rebuilt == derivedAbs);
        controls.checkBox->setEnabled(false);
    } else if(type == AnswerStore::TypeAutoChainTypst)
    {
        // 链卡(typst)：用户只输入公式体（两端 $ 不敲），与宾词的公式体逐字相等即判对
        // （图谱里 `$ x $` 与 `$x$` 是同一答案：两端 $ 与紧贴的空白不参与比较）
        const QString derivedAbs = questionSql->get_value(kQuestionsTable, currentQuestionId, kAbstractNameField).toString();
        const QString objectText = KnowledgeDB::lastObjectOfDerivedAbs(derivedAbs);
        controls.checkBox->setChecked(!derivedAbs.isEmpty() && KnowledgeDB::isChainTypstName(objectText)
                                      && input.trimmed() == KnowledgeDB::chainTypstBodyOf(objectText));
        controls.checkBox->setEnabled(false);
    }
}

// 乱序池行：输入与池内"自动检查"选项按内容匹配并占用去重；判分跟随本行**实际占用的选项**（自动→机器判，手动→交还自判）
void LearningDialog::revealPoolAnswerRow(int row, const QJsonObject &answer, const QJsonArray &answers)
{
    QComboBox *answerComboBox = answerComboAt(row);
    if(!answerComboBox)
        return;
    AnswerRowControls controls = answerRowAt(row);
    const bool notStudiedYet = controls.timeLabel && ToolFunctions::isUnstudied(controls.timeLabel->text());
    alignCompareLabels(controls); // 与普通行同一口径：按输入框内容区左内缩对齐对照标签
    if(!notStudiedYet && controls.answerSourceLabel) // typst 池行：原文随答案一同揭示
        controls.answerSourceLabel->show();
    controls.checkBox->setChecked(false);
    const int matchedItem = matchedPoolItem(answerComboBox, controls.input->text(), answers);
    if(matchedItem >= 0)
    {
        controls.checkBox->setChecked(true);
        answerComboBox->setCurrentIndex(matchedItem);
        removeMatchedPoolOption(answerComboBox, row, answer, answers);
    }
    const QString consumedType = consumedPoolAnswer(answerComboBox, answers).value(AnswerStore::FieldType).toString();
    if(isAutoCheckedAnswer(consumedType))
        controls.checkBox->setEnabled(false); // 占用自动选项：命中即判对、未命中即判错，不允许改判
    else
        showManualRatingRow(row); // 占用手动选项：交还自判，并给出四档自评
    answerComboBox->show();
    if(controls.timeLabel && ToolFunctions::isUnstudied(controls.timeLabel->text()))
        answerComboBox->hide(); // 从未学过的空不揭示答案
}

// 已匹配的池选项从同池其他选框中移除（每个选项只能用一次；按 itemData 里的池行号定位，避免空文本/同文本项误删）
void LearningDialog::removeMatchedPoolOption(QComboBox *matchedComboBox, int row, const QJsonObject &answer, const QJsonArray &answers)
{
    matchedComboBox->setEnabled(false);
    const int pool = answer.value(AnswerStore::FieldPool).toInt();
    const QVariant matchedPoolRow = matchedComboBox->currentData();
    for(int otherRow = 0; otherRow < answerRowCount; ++otherRow)
    {
        if(otherRow == row)
            continue;
        if(answers[otherRow].toObject().value(AnswerStore::FieldPool).toInt() != pool)
            continue;
        QComboBox *otherComboBox = answerComboAt(otherRow);
        if(otherComboBox)
            otherComboBox->removeItem(otherComboBox->findData(matchedPoolRow));
    }
}

void LearningDialog::submit()
{
    answerStage = AnswerStage::Submitted;

    if(gateActive) // 链卡行：正式提交才做门控处理（记录 + 回填 + 离表；组满则立刻组大提交），普通答案区的判分与写库全部跳过
    {
        gateRecordAnswer();
        return;
    }

    const QJsonArray answers = answerStore->answers(currentQuestionId);
    bool wrong = false;
    QSqlDatabase database = questionSql->getDb();
    const bool transactionStarted = database.transaction(); // 逐行写入与题目聚合必须原子：中途失败会留下"部分行已推进"的节奏不一致
    for(int row = 0; row < answerRowCount; ++row)
    {
        if(!rowParticipates(row))
            continue; // 未到期的空不参与作答
        writeAnswerRowState(row, answers[row].toObject(), answers, wrong);
    }
    applyRatingToSummaryRow(wrong);
    questionSql->update_question_state(currentQuestionId);
    if(transactionStarted && !database.commit())
    {
        database.rollback();
        QMessageBox::warning(this, QStringLiteral("提交失败"), database.lastError().text());
        answerStage = AnswerStage::PreSubmitted; // 未落盘：保持已揭示态，下一次点击重试提交
        return;
    }
    leaveAnsweredQuestionRow(wrong);
}

// 写一行 FSRS 状态（乱序池按选框定实际 aid；未学过的空跳过）并按行计入正确率；anyRowWrong 回传本题是否出现过答错（供汇总行结论判定）
void LearningDialog::writeAnswerRowState(int row, const QJsonObject &answer, const QJsonArray &answers, bool &anyRowWrong)
{
    AnswerRowControls controls = answerRowAt(row);
    if(!controls.checkBox || !controls.timeLabel)
        return;
    controls.checkBox->setEnabled(false);
    if(controls.ratingBox)
        controls.ratingBox->setEnabled(false); // 已结算：自评档位不再可改（改了也不会重写）

    int answerId = answer.value(AnswerStore::FieldId).toInt();
    if(answer.value(AnswerStore::FieldPool).toInt() != 0) // 乱序池：实际 aid 由当前选中项决定
    {
        QComboBox *answerComboBox = answerComboAt(row);
        if(answerComboBox)
            answerId = answers[answerComboBox->itemData(answerComboBox->currentIndex()).toInt()]
                           .toObject().value(AnswerStore::FieldId).toInt();
    }

    const QString chosenRating = chosenManualRating(row);
    if(chosenRating.isEmpty() && ToolFunctions::isUnstudied(controls.timeLabel->text()))
        return; // 未学过的空且未自评：不更新状态、不计入统计（自评即用户已engage，照常记账）
    QTime answerTime = ToolFunctions::msz2QTime(controls.timeLabel->text());
    const bool rowWrong = !controls.checkBox->isChecked(); // 勾选框已与自评档位同步
    if(rowWrong)
    {
        anyRowWrong = true;
        answerTime = QTime::fromMSecsSinceStartOfDay(0); // 答错按 0 用时记（当天重来）
    }
    if(!rowWrong && !chosenRating.isEmpty())
        questionSql->update_cloze_state_by_rating(answerId, answerTime, chosenRating); // manual 自评：评分取自用户
    else
        questionSql->update_cloze_state(answerId, answerTime);
    updateAccuracyLabel(rowWrong ? 0 : 1, 1);
}

// 汇总行结论：manual 自评档位优先，其次按用时与良好时间评级（答错一律 出错）
void LearningDialog::applyRatingToSummaryRow(bool wrong)
{
    QLabel *goodTimeLabel = summaryGoodTimeLabel();
    QLabel *ratingLabel = summaryCheckLabel();
    if(!ratingLabel)
        return;
    const QString chosenRating = firstChosenManualRating();
    if(wrong)
    {
        answerElapsed = QTime::fromMSecsSinceStartOfDay(0);
        ratingLabel->setText(kRatingWrongText);
    } else if(!chosenRating.isEmpty())
    {
        ratingLabel->setText(ratingTextOf(chosenRating));
    } else
    {
        const QTime goodTime = QTime::fromString(goodTimeLabel ? goodTimeLabel->text() : QString(), kGoodTimeFormat); // 汇总行良好时间按库内格式写入
        ratingLabel->setText(ratingTextOf(FSRS::time2rating(answerElapsed, goodTime)));
    }
    ratingLabel->show();
}

// 提交后收尾：速学下答对即离表、刷新表并保持当前行
void LearningDialog::leaveAnsweredQuestionRow(bool wrong)
{
    lastAnsweredRowIndex = ui->tableView->currentIndex().row();
    if(isSpeedLearn && !wrong) // 速学：答对的题从本轮移除
        questionTableModel->setFilter(ToolFunctions::andFilters(
            questionTableModel->filter(), QStringLiteral("questions.id != %1").arg(currentQuestionId)));
    reloadQuestionTable();
    refreshAnswerMasks(); // 良好完成后本题离表（速学追加过滤），其知识行遮罩在此恢复
    if(lastAnsweredRowIndex < 0 || lastAnsweredRowIndex >= questionTableModel->rowCount())
        lastAnsweredRowIndex = 0;
    ui->tableView->selectRow(lastAnsweredRowIndex);
}

void LearningDialog::on_pushButton_clicked()
{
    stop_timer();

    if(questionTableModel->rowCount() == 0)
    {
        totalTimer->stop();
        return;
    }
    if(currentQuestionId == -1)
        return;

    if(answerStage == AnswerStage::Submitted) // 三次点击语义：未提交 → 预提交（锁定输入）→ 正式提交（写库）→ 再点击切换下一题
    {
        if(gatePendingRollGroup != kNoGroup) // 链卡行已正式提交：本次点击才切到同组下一未答行
        {
            const int groupIndex = gatePendingRollGroup;
            gatePendingRollGroup = kNoGroup;
            gateReviewNextRow(groupIndex);
            return;
        }
        advanceToNextQuestion();
        return;
    }
    if(answerStage == AnswerStage::Waiting)
        preSubmit();
    else
        submit();
}

// 提交后的切题与默认排序（advanceToNextQuestion / rescheduleAnsweredRowForDefaultOrder）已移到 learningdialog.cpp