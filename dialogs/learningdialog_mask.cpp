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

bool LearningDialog::questionHasChainCard(int questionId)
{
    auto cached = chainCardByQId.constFind(questionId);
    if(cached != chainCardByQId.constEnd())
        return cached.value();
    const bool found = AnswerStore::hasChainCard(answerStore->answers(questionId));
    chainCardByQId.insert(questionId, found);
    return found;
}

// 本次要遮成 "?" 的 DerivedAbs 集合 = 题目表内含链卡的题的 DerivedAbs ∪ 本轮全部链卡行（其余组的行虽暂不在表内也不能露名字）
QSet<QString> LearningDialog::collectMaskedDerivedAbs()
{
    QSet<QString> maskedDerivedAbs;
    const int absCol = QuestionSql::headerColumnMap[kAbstractNameField];
    for(int row = 0; row < questionTableModel->rowCount(); ++row)
    {
        const QString derivedAbs = questionTableModel->index(row, absCol).data().toString();
        if(!derivedAbs.isEmpty() && questionHasChainCard(questionTableModel->index(row, 0).data().toInt()))
            maskedDerivedAbs.insert(derivedAbs);
    }
    for(const ChainCard &card : gateCards)
        maskedDerivedAbs.insert(card.derivedAbs);
    return maskedDerivedAbs;
}

// 单个树行对账：需要遮 → 宾词变 "?"（保留关系段，⇒ 合并行改写 base/full 以支持展开/折叠），已遮但不再需要 → 还原备份
void LearningDialog::applyMaskToTreeItem(QStandardItem *item, const QSet<QString> &maskedDerivedAbs, QTreeView *view)
{
    const QString derivedAbs = item->data(DerivedAbsRole).toString();
    const bool merged = item->data(MergeFlagRole).toBool();
    const QString onlyChildDerivedAbs = (merged && item->rowCount() == 1)
                                            ? item->child(0)->data(DerivedAbsRole).toString()
                                            : QString();
    const bool maskSelf = !derivedAbs.isEmpty() && maskedDerivedAbs.contains(derivedAbs);                 // 题目挂在本行 DerivedAbs 上
    const bool maskChildSuffix = !onlyChildDerivedAbs.isEmpty() && maskedDerivedAbs.contains(onlyChildDerivedAbs); // 题目挂在本 ⇒ 合并行的唯一 is 子行上
    const bool wantMask = maskSelf || maskChildSuffix;

    if(wantMask && !item->data(HasOrigRole).toBool()) // 备份只在首次遮罩时做，模型重建后随 item 重置
    {
        item->setData(true, HasOrigRole);
        item->setData(item->data(Qt::DisplayRole).toString(), OrigDisplayRole);
        if(merged)
        {
            item->setData(item->data(MergeBaseRole).toString(), OrigBaseRole);
            item->setData(item->data(MergeFullRole).toString(), OrigFullRole);
        }
    }
    if(!wantMask && !item->data(MaskedRole).toBool())
        return;

    const QString origBase = item->data(OrigBaseRole).toString();
    const QString origSuffix = item->data(OrigFullRole).toString().mid(origBase.length());
    const QString base = wantMask ? (maskSelf ? item->data(RelationPrefixRole).toString() + kMaskedBlankText : origBase)
                                  : origBase;
    const QString full = wantMask ? base + (maskChildSuffix ? kTreeMergeArrow + kMaskedBlankText : origSuffix)
                                  : item->data(OrigFullRole).toString();
    if(!merged)
    {
        item->setData(wantMask ? base : item->data(OrigDisplayRole).toString(), Qt::DisplayRole);
    } else
    {
        item->setData(base, MergeBaseRole);
        item->setData(full, MergeFullRole);
        item->setData(view && view->isExpanded(item->index()) ? base : full, Qt::DisplayRole);
    }
    item->setData(wantMask, MaskedRole);
}

// 对某个知识树模型全量对账（主树与宾词视图共用；⇒ 合并行的展开态判定要看各自的 view）
void LearningDialog::applyAnswerMasksTo(QStandardItemModel *model, QTreeView *view, const QSet<QString> &maskedDerivedAbs)
{
    if(!model || model->rowCount() == 0)
        return;
    std::function<void(QStandardItem *)> walk = [&](QStandardItem *item)
    {
        if(!item)
            return;
        if(item->data(ChainDrillStateRole).toInt() == kDrillStateNone) // 演练现场行（含 ⇒ 合并父行镜像）跳过对账
            applyMaskToTreeItem(item, maskedDerivedAbs, view);
        for(int i = 0; i < item->rowCount(); ++i)
            walk(item->child(i));
    };
    for(int i = 0; i < model->rowCount(); ++i)
        walk(model->item(i));
}

// 按当前复习集合全量对账两个知识模型（主树 + 宾词视图）的遮罩/还原；门控已回填/揭示的行保留现场不参与
void LearningDialog::refreshAnswerMasks()
{
    if((!knowledgeModel || knowledgeModel->rowCount() == 0)
       && (!phenObjModel || phenObjModel->rowCount() == 0))
        return;
    const QSet<QString> maskedDerivedAbs = collectMaskedDerivedAbs();
    applyAnswerMasksTo(knowledgeModel, ui->phenTreeView, maskedDerivedAbs);
    applyAnswerMasksTo(phenObjModel, ui->phenObjTreeView, maskedDerivedAbs);
}