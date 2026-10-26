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
const QString kCorrectRevealColor = QStringLiteral("#2e7d32");
const QString kWrongRevealColor = QStringLiteral("#c62828");
const QString kGateInputHint = QStringLiteral("填写本行概念名（回车提交）");
const QString kGateFormulaInputHint = QStringLiteral("填写公式体，不需要两端 $（回车提交）");

// 递归禁用并隐藏一个格位里的全部件：嵌套布局（输入行 / 对照行）要逐层下探，只处理顶层的 widget 会漏
void disableAndHideLayoutWidgets(QLayoutItem *item)
{
    if(!item)
        return;
    if(QWidget *widget = item->widget())
    {
        widget->setEnabled(false);
        widget->hide();
    }
    if(QLayout *layout = item->layout())
        for(int i = 0; i < layout->count(); ++i)
            disableAndHideLayoutWidgets(layout->itemAt(i));
}
}

int LearningDialog::questionIdByDerivedAbsName(const QString &derivedAbs) const
{
    if(derivedAbs.isEmpty())
        return kNoQuestion;
    QSqlQuery query(questionSql->getDb());
    query.prepare(QStringLiteral("SELECT id FROM questions WHERE abstractName = ?"));
    query.addBindValue(derivedAbs);
    if(query.exec() && query.next())
        return query.value(0).toInt();
    return kNoQuestion;
}

// 链卡(typst) 的归属兜底：公式体没有唯一拼法（`$ x $` / `$x$` 等价），只能全表取每条 abstractName 现算公式体比对
int LearningDialog::questionIdByChainTypstBody(const QString &body) const
{
    if(body.isEmpty())
        return kNoQuestion;
    QSqlQuery query(questionSql->getDb());
    query.prepare(QStringLiteral("SELECT id, abstractName FROM questions"));
    if(!query.exec())
        return kNoQuestion;
    while(query.next())
        if(KnowledgeDB::chainTypstBodyOf(KnowledgeDB::lastObjectOfDerivedAbs(query.value(1).toString())) == body)
            return query.value(0).toInt();
    return kNoQuestion;
}

QStandardItem *LearningDialog::findKnowledgeItemByDerivedAbs(const QString &derivedAbs)
{
    if(derivedAbs.isEmpty() || !knowledgeModel || knowledgeModel->rowCount() == 0)
        return nullptr;
    const auto hits = knowledgeModel->match(knowledgeModel->index(0, 0), DerivedAbsRole,
                                            derivedAbs, 1, Qt::MatchExactly | Qt::MatchRecursive);
    return hits.isEmpty() ? nullptr : knowledgeModel->itemFromIndex(hits.first());
}

// 门控行锁判定：演练期间锁定的门控行（其展开被禁用、子行不显示）
bool LearningDialog::isDerivedAbsLocked(const QModelIndex &index) const
{
    return index.isValid() && gateLockDerivedAbs.contains(index.data(DerivedAbsRole).toString());
}

// 链卡行进入直接作答态：只启用标题 auto(chain) 输入框（其余空禁用+隐藏、底部汇总行隐藏）、题面只保留首行作语境（防泄漏），该行强制折叠（子行不显示，与题目表剔除保持两表一致）
void LearningDialog::enterGateMode(int questionId)
{
    gateActive = false;
    if(!gateCardIds.contains(questionId) || gateAnsweredIds.contains(questionId))
        return;

    gateChainRow = kNoGateRow; // 链卡所在答案行（每题至多一张链卡）
    for(int row = 0; row < currentAnswers.size(); ++row)
        if(AnswerStore::isChainCardType(currentAnswers[row].toObject().value(AnswerStore::FieldType).toString()))
        {
            gateChainRow = row;
            break;
        }
    if(gateChainRow < 0)
        return;

    gateInput = answerRowAt(gateChainRow).input;
    if(!gateInput)
        return;
    gateInput->clear();
    gateInput->setEnabled(true);
    // 链卡(typst)只输入公式体（两端 $ 由回传补），提示语据类型区分
    gateInput->setPlaceholderText(currentAnswers[gateChainRow].toObject().value(AnswerStore::FieldType).toString()
                                          == AnswerStore::TypeAutoChainTypst
                                      ? kGateFormulaInputHint
                                      : kGateInputHint);

    hideNonChainAnswerRows(answerGrid());
    keepOnlyFirstQuestionLine(questionId);

    const QString gateDerivedAbs = questionSql->get_value(kQuestionsTable, questionId, kAbstractNameField).toString();
    if(QStandardItem *gateItem = findKnowledgeItemByDerivedAbs(gateDerivedAbs)) // 强制折叠：子行不显示（锁定展开由守卫负责）
        ui->phenTreeView->collapse(gateItem->index());
    gateActive = true;
    gateInput->setFocus();
    timer->start();
}

// 本题只考标题链卡：其余答案行与底部汇总行一律禁用并隐藏
void LearningDialog::hideNonChainAnswerRows(QGridLayout *layout)
{
    for(int row = 0; row < answerRowCount; ++row)
    {
        if(row == gateChainRow)
            continue;
        // 输入列现在是"输入行 + 对照行"的纵向容器：逐层下探，禁用并隐藏其中的全部件
        disableAndHideLayoutWidgets(layout->itemAtPosition(row, ColumnInput));
        for(int column : {ColumnLineNumber, ColumnCheck, ColumnTime, ColumnGoodTime, ColumnRating})
            if(QLayoutItem *item = layout->itemAtPosition(row, column))
                if(QWidget *widget = item->widget())
                {
                    widget->setEnabled(false);
                    widget->hide();
                }
    }
    for(int column : {ColumnCheck, ColumnTime, ColumnGoodTime}) // 底部整题汇总行（结论/总计时/良好）同样隐藏
        if(QLayoutItem *item = layout->itemAtPosition(answerRowCount, column))
            if(QWidget *widget = item->widget())
                widget->hide();
}

// 题面只保留首行（链卡所在标题行），其余文本隐藏作防泄漏语境
void LearningDialog::keepOnlyFirstQuestionLine(int questionId)
{
    QTextDocument fullDocument;
    fullDocument.setHtml(questionSql->read_questionHTML(questionId));
    QTextDocument firstLineDocument;
    const QTextBlock firstBlock = fullDocument.firstBlock();
    if(firstBlock.isValid())
    {
        QTextCursor source(&fullDocument);
        source.setPosition(firstBlock.position());
        source.setPosition(firstBlock.position() + firstBlock.length() - 1, QTextCursor::KeepAnchor);
        QTextCursor target(&firstLineDocument);
        target.insertFragment(source.selection());
    }
    ui->textBrowser->setHtml(firstLineDocument.toHtml());
}

// 门控演练树行写入：objectText 为对象段文本（"?"/用户输入/揭示文本），drillState：0=恢复遮罩态/1=回填/2=正确揭示（绿）/3=错误揭示（红）；⇒ 合并行的唯一 is 子行更新时同步镜像父行后缀与颜色，折叠态不回弹、不泄漏
void LearningDialog::writeGateRowText(const QString &derivedAbs, const QString &objectText, int drillState)
{
    if(drillState == kDrillStateNone) // 先记快照：模型重建（检索）后据此恢复现场；item 可能暂时不在视图中
        gateDrillView.remove(derivedAbs);
    else
        gateDrillView.insert(derivedAbs, qMakePair(objectText, drillState));

    QStandardItem *item = findKnowledgeItemByDerivedAbs(derivedAbs);
    if(!item)
        return;
    applyDrillStateToItem(item, objectText, drillState);
    mirrorDrillStateToMergedParent(item, objectText, drillState);
}

// ⇒ 合并行的展开/折叠文本（折叠态显示带 " ⇒ …" 后缀的完整文本）
void LearningDialog::setTreeRowText(QStandardItem *item, const QString &base, const QString &full)
{
    item->setData(base, MergeBaseRole);
    item->setData(full, MergeFullRole);
    item->setData(ui->phenTreeView->isExpanded(item->index()) ? base : full, Qt::DisplayRole);
}

// 演练行颜色：正确=绿、错误=红、回填/还原=默认前景色
void LearningDialog::applyDrillColor(QStandardItem *item, int drillState)
{
    if(drillState == kDrillStateCorrect)
        item->setForeground(QBrush(QColor(kCorrectRevealColor)));
    else if(drillState == kDrillStateWrong)
        item->setForeground(QBrush(QColor(kWrongRevealColor)));
    else
        item->setData(QVariant(), Qt::ForegroundRole);
}

void LearningDialog::applyDrillStateToItem(QStandardItem *item, const QString &objectText, int drillState)
{
    const QString rowText = item->data(RelationPrefixRole).toString() + objectText;
    item->setData(drillState == kDrillStateNone ? QVariant() : QVariant(drillState), ChainDrillStateRole);
    applyDrillColor(item, drillState);
    if(item->data(MergeFlagRole).toBool()) // 本行自身是 ⇒ 合并行：保留其现有 " ⇒ …" 后缀
    {
        const QString currentBase = item->data(MergeBaseRole).toString();
        const QString currentFull = item->data(MergeFullRole).toString();
        const QString suffix = currentFull.size() > currentBase.size() ? currentFull.mid(currentBase.size()) : QString();
        setTreeRowText(item, rowText, rowText + suffix);
    } else
    {
        item->setData(rowText, Qt::DisplayRole);
    }
}

// 若本行是其 ⇒ 合并父行的唯一 is 子行，镜像父行（折叠态显示"父 ⇒ 输入/揭示"）
void LearningDialog::mirrorDrillStateToMergedParent(QStandardItem *item, const QString &objectText, int drillState)
{
    QStandardItem *parent = item->parent();
    if(!parent || !parent->data(MergeFlagRole).toBool() || parent->rowCount() != 1)
        return;
    parent->setData(drillState == kDrillStateNone ? QVariant() : QVariant(drillState), ChainDrillStateRole);
    applyDrillColor(parent, drillState);
    const QString parentBase = parent->data(MergeBaseRole).toString();
    setTreeRowText(parent, parentBase, parentBase + kTreeMergeArrow + objectText);
}

// 模型重建（检索）后按快照恢复演练现场（回填/揭示文本与颜色）
void LearningDialog::reapplyGateDrillView()
{
    const auto snapshot = gateDrillView; // writeGateRowText 会写回 map，先拷副本再遍历
    for(auto entry = snapshot.cbegin(); entry != snapshot.cend(); ++entry)
        writeGateRowText(entry.key(), entry.value().first, entry.value().second);
}