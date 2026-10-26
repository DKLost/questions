#include "knowledgetreehelper.h"

#include "utils/toolfunctions.h"
#include "widgets/typstrowdelegate.h"

#include <QDate>
#include <QHash>
#include <QHeaderView>
#include <QSet>
#include <QEvent>
#include <QMouseEvent>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QStandardItem>
#include <algorithm>
#include <functional>

namespace {
constexpr int kPhenomenonColumn = 0;
constexpr int kAtFlagColumn = 1;
constexpr int kDueColumn = 2; // 到期天数：负数为已过期，空为未挂题

// 结构名与谓词记号
const QString kWorldName = QStringLiteral("world");
const QString kHasPredicate = QStringLiteral("has");
const QString kAtHasPredicate = QStringLiteral("~has");
const QString kIsPredicate = QStringLiteral("is");
const QString kAtMarker = QStringLiteral("@");
const QString kAtPrefix = QStringLiteral("@ ");
const QString kRelationSeparator = QStringLiteral(" : ");
const QString kAtRelationSeparator = QStringLiteral(" @ ");
const QString kMergeArrow = QStringLiteral(" ⇒ ");
const QChar kGroupKeySeparator(0x1f);  // 分组键里主词与关系段的分隔（不会出现在 DerivedAbs 名中）

// 到期列：题目表 abstractName 命中本行 DerivedAbs 时显示该题的"到期天数"（未挂题留空）
const QString kQuestionDbConnection = QStringLiteral("connection1"); // core 里 QuestionSql 的连接名
const QString kQuestionDueDateSql = QStringLiteral("SELECT abstractName, nextDate FROM questions WHERE abstractName IS NOT NULL");
const QString kDateStorageFormat = QStringLiteral("yyyy/MM/dd"); // questions.nextDate 在库中的格式

QHash<QString, QString> &questionDueDatesSnapshot()
{
    static QHash<QString, QString> nextDates;
    return nextDates;
}

// 刷新"DerivedAbs → 题目到期日"快照：建树（或复习结束刷新）前查一次题目表，供到期列逐行计算天数
void refreshQuestionDueDates()
{
    QHash<QString, QString> &nextDates = questionDueDatesSnapshot();
    nextDates.clear();
    QSqlQuery query(QSqlDatabase::database(kQuestionDbConnection));
    query.exec(kQuestionDueDateSql);
    while(query.next())
        nextDates.insert(query.value(0).toString(), query.value(1).toString());
}

// 到期列的显示文本：今日到期 0、明日 1、昨日 -1（负数为已过期）；该 DerivedAbs 未挂题返回空
QString questionDueDaysText(const QString &derivedAbs)
{
    const QHash<QString, QString> &nextDates = questionDueDatesSnapshot();
    if(derivedAbs.isEmpty() || !nextDates.contains(derivedAbs))
        return {}; // 未挂题：不显示天数
    const QDate nextDate = QDate::fromString(nextDates.value(derivedAbs), kDateStorageFormat);
    const int days = nextDate.isValid() ? static_cast<int>(QDate::currentDate().daysTo(nextDate)) : 0;
    return QString::number(days); // 无有效排期的题按"今天到期"显示
}

QStandardItemModel *itemModel(QTreeView *view)
{
    return view ? qobject_cast<QStandardItemModel *>(view->model()) : nullptr;
}

void clearModelForKnowledge(QStandardItemModel *model)
{
    model->clear();
    model->setHorizontalHeaderLabels({QStringLiteral("现象"), kAtMarker, QStringLiteral("到期")});
}
}

TreeNode *fillKnowledgeModel(QStandardItemModel *model)
{
    KnowledgeDB *db = KnowledgeDB::instance();
    clearModelForKnowledge(model);
    refreshQuestionDueDates();

    const int worldId = db->ensureAbstract(Abstract{kWorldName});
    TreeNode *root = db->buildTree(worldId);

    QStandardItem *modelRoot = model->invisibleRootItem(); // world 根本身不显示，其孩子直接挂到模型根
    for(TreeNode *child : root->children)
        appendKnowledgeNode(child, modelRoot);

    return root;
}

// 检索主体/谓词：world 裸谓词直查；非 world 主体 qualify、谓词取 rEAbs（只命中 : 行）
void resolveFindTargets(const QString &subj, const QString &pred, QString *querySubj, QString *queryPred)
{
    KnowledgeDB *db = KnowledgeDB::instance();
    if(subj == kWorldName)
    {
        *querySubj = kWorldName;
        *queryPred = pred;
    } else
    {
        *querySubj = db->qualify(subj);
        *queryPred = db->rExistAbsPredicate(Abstract{pred}).name;
    }
}

// 命中名的"身份链"：自 world 顶层概念一路到该名自身，每级都是树中某行的身份。
// 如 world→has:[甲]→has:[world→has:[乙]] → [world→has:[甲], 该名]；顶层名自身即链首
static QStringList identityChainOf(const QString &name)
{
    KnowledgeDB *db = KnowledgeDB::instance();
    QStringList chain;
    QString cur = name;
    while(true)
    {
        chain.prepend(cur);
        QString subject;
        if(!ToolFunctions::splitDerivedAbsTail(cur, &subject, nullptr))
            break; // 叶子名：链到此为止
        if(subject == kWorldName)
            break; // 本名已是 world 顶层概念的身份
        if(db->isQualified(subject))
        {
            chain.prepend(subject); // 顶层概念名：链首
            break;
        }
        cur = subject; // 继续向上一层主体链
    }
    return chain;
}

// 沿身份链自顶下探，逐级落到树节点（同身份多行时优先 : 行；镜像 @ 行只在没有 : 行时才用）
static bool followIdentityChain(const TreeNode *from, const QStringList &chain, QList<const TreeNode *> *path)
{
    const TreeNode *cur = from;
    for(const QString &identity : chain)
    {
        const TreeNode *next = nullptr;
        for(const TreeNode *child : cur->children)
        {
            if(child->derivedAbs != identity)
                continue;
            if(!child->atFlag)
            {
                next = child;
                break;
            }
            if(!next)
                next = child;
        }
        if(!next)
            return false; // 链缺环（异常数据）：该命中不显示
        cur = next;
        path->append(cur);
    }
    return true;
}

static void markWholeSubtree(const TreeNode *node, QSet<const TreeNode *> *kept)
{
    kept->insert(node);
    for(const TreeNode *child : node->children)
        markWholeSubtree(child, kept);
}

// 只呈现检索命中的概念：命中名自身编码了"到它为止的身份链"，按链逐级补出父行（未命中的兄弟剪掉），
// 命中行以下照常整棵展开；多个命中共用同一前缀时自然合并到同一父行
void fillMatchedNodes(TreeNode *root, const QList<Abstract> &hits, const QString &obj, bool objExactMatch,
                      QStandardItem *modelRoot)
{
    KnowledgeDB *db = KnowledgeDB::instance();
    QSet<const TreeNode *> kept;
    for(const Abstract &hit : hits)
    {
        // 宾词比**显示名**：库内名对深节点是限定名/链名，直接比符号名会把 world/has 等结构段也算进去
        if(!obj.isEmpty())
        {
            const QString display = db->unqualify(hit.name);
            const bool objMatched = objExactMatch ? display == obj // 宾词视图：宾词取自树行本身，只认这一个概念
                                                  : display.contains(obj, Qt::CaseInsensitive);
            if(!objMatched)
                continue;
        }
        QList<const TreeNode *> path;
        if(!followIdentityChain(root, identityChainOf(hit.name), &path))
            continue;
        const TreeNode *hitNode = path.last();
        if(hitNode->atFlag) // @ 行不是 : 行宾语在树中的落点（互为正反陈述的两条线可同派生名，撞上会多出假分支）
            continue;
        for(const TreeNode *ancestor : path)
            kept.insert(ancestor);
        markWholeSubtree(hitNode, &kept);
    }
    for(TreeNode *child : root->children) // world 根本身不显示，其孩子直接挂到模型根（同 fillKnowledgeModel）
        if(kept.contains(child))
            appendKnowledgeNode(child, modelRoot, &kept);
}

TreeNode *fillFindObjsModel(QStandardItemModel *model, const QString &subjectInput, const QString &predicateInput,
                            const QString &objectInput, bool objExactMatch)
{
    const QString subject = KnowledgeDB::encodeName(subjectInput.trimmed().isEmpty() ? kWorldName : subjectInput.trimmed()); // 主词与谓词按名字层转义后匹配库内名（空输入取默认 world/has）
    const QString predicate = KnowledgeDB::encodeName(predicateInput.trimmed().isEmpty() ? kHasPredicate : predicateInput.trimmed());
    const QString object = objectInput.trimmed(); // 宾词与显示名同处一侧，不转义（见 fillMatchedNodes）

    QString querySubj;
    QString queryPred;
    resolveFindTargets(subject, predicate, &querySubj, &queryPred);
    const QList<Abstract> hits = KnowledgeDB::instance()->findObjects(queryPred, querySubj);

    const int worldId = KnowledgeDB::instance()->ensureAbstract(Abstract{kWorldName}); // 统一建 world 全树再筛选
    TreeNode *root = KnowledgeDB::instance()->buildTree(worldId);
    clearModelForKnowledge(model);
    refreshQuestionDueDates();
    if(!hits.isEmpty())
        fillMatchedNodes(root, hits, object, objExactMatch, model->invisibleRootItem());
    return root;
}

// 宾词前的关系段：has/~has 省略，其余标注 (∃)形式（: 行取 rev(unfold_L)、@ 行取 unfold_R）
QString relationPrefixOf(const TreeNode *node)
{
    KnowledgeDB *db = KnowledgeDB::instance();
    if(!node->atFlag)
        return node->revUnfoldedPred == kHasPredicate ? QString()
                                                      : db->unqualify(node->revUnfoldedPred) + kRelationSeparator;
    return node->unfoldedPred == kAtHasPredicate ? kAtPrefix
                                                 : db->unqualify(node->unfoldedPred) + kAtRelationSeparator;
}

// ⇒ 合并：本节点恰有一条 is 子边时折叠显示“关系 ⇒ X”（: / @ 同一套规则）
bool mergeTextOf(const TreeNode *node, const QString &label, QString *baseText, QString *fullText)
{
    if(node->children.size() != 1)
        return false;
    const TreeNode *childNode = node->children.first();
    const QString childRelation = childNode->atFlag ? childNode->unfoldedPred : childNode->revUnfoldedPred;
    if(childRelation != kIsPredicate)
        return false;
    *baseText = label;
    *fullText = label + kMergeArrow + KnowledgeDB::instance()->unqualify(childNode->objectName);
    return true;
}

void appendKnowledgeNode(TreeNode *node, QStandardItem *parent, const QSet<const TreeNode *> *kept)
{
    KnowledgeDB *db = KnowledgeDB::instance();
    const QString relationPrefix = relationPrefixOf(node);
    QString label = relationPrefix + db->unqualify(node->objectName);
    QString baseText;
    QString fullText;
    const bool merged = mergeTextOf(node, label, &baseText, &fullText);
    if(merged)
        label = fullText; // 建表时节点默认折叠

    QList<QStandardItem *> row{
        new QStandardItem(label),
        new QStandardItem(node->atFlag ? kAtMarker : QString()),
        new QStandardItem(questionDueDaysText(node->derivedAbs))};

    row[kPhenomenonColumn]->setData(node->derivedAbs, DerivedAbsRole); // buildTree 预算：: 行 S→L:[O]、@ 行 O→R:[S]
    row[kPhenomenonColumn]->setData(relationPrefix, RelationPrefixRole);
    row[kDueColumn]->setData(node->phenId, PhenIdRole); // id 不上屏，只作展开集/定位的键
    if(merged)
    {
        row[kPhenomenonColumn]->setData(true, MergeFlagRole);
        row[kPhenomenonColumn]->setData(baseText, MergeBaseRole);
        row[kPhenomenonColumn]->setData(fullText, MergeFullRole);
    }
    parent->appendRow(row);

    for(TreeNode *child : node->children) // 子树挂到本行，保持 buildTree 的嵌套层级
    {
        if(kept && !kept->contains(child)) // 检索剪枝：不在命中路径上的兄弟行不显示（⇒ 合并判定仍按真实子行数，避免表示失真）
            continue;
        appendKnowledgeNode(child, row[kPhenomenonColumn], kept);
    }
}

void setupKnowledgeTreeView(QTreeView *view)
{
    view->setColumnHidden(kAtFlagColumn, true); // @ (∃)形式已在行文本中体现
    view->header()->setStretchLastSection(false);
    view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    view->header()->setSectionResizeMode(kPhenomenonColumn, QHeaderView::Stretch);
    // 行内 typst 片段（$ … $）就地渲染成图片；只装文本列，且重复调用不叠加（本函数会随模型重建被反复调用）
    if(!qobject_cast<TypstRowDelegate *>(view->itemDelegateForColumn(kPhenomenonColumn)))
        view->setItemDelegateForColumn(kPhenomenonColumn, new TypstRowDelegate(view));
}

void installReadOnlyKnowledgeTree(QTreeView *view)
{
    setupKnowledgeTreeView(view);
    installMergeToggle(view);
    installMiddleClickExpandAllExceptMerged(view);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setContextMenuPolicy(Qt::NoContextMenu);
}

void installMergeToggle(QTreeView *view)
{
    QObject::connect(view, &QTreeView::expanded, view, [view](const QModelIndex &index) {
        if(index.data(MergeFlagRole).toBool())
            if(QStandardItemModel *model = itemModel(view))
                model->setData(index, index.data(MergeBaseRole), Qt::DisplayRole);
    });
    QObject::connect(view, &QTreeView::collapsed, view, [view](const QModelIndex &index) {
        if(index.data(MergeFlagRole).toBool())
            if(QStandardItemModel *model = itemModel(view))
                model->setData(index, index.data(MergeFullRole), Qt::DisplayRole);
    });
}

// 新选择是否仍在 ⇒ 合并父行子树内（深入 is 行自身子树或点中父行都算）
bool staysInMergeSubtree(const QModelIndex &current, const QModelIndex &mergeParent)
{
    for(QModelIndex ancestor = current; ancestor.isValid(); ancestor = ancestor.parent())
        if(ancestor == mergeParent)
            return true;
    return false;
}

void installCollapseMergeOnLeave(QTreeView *view)
{
    QItemSelectionModel *selectionModel = view->selectionModel();
    if(!selectionModel)
        return; // 须先 setModel
    QObject::connect(selectionModel, &QItemSelectionModel::currentChanged, view,
                     [view](const QModelIndex &current, const QModelIndex &previous) {
        if(!previous.isValid())
            return;
        const QModelIndex mergeParent = previous.parent(); // previous 须是 ⇒ 合并父行下的 is 子行
        if(!mergeParent.isValid() || !mergeParent.data(MergeFlagRole).toBool())
            return;
        if(!staysInMergeSubtree(current, mergeParent))
            view->collapse(mergeParent);
    });
}

void expandAllExceptMerged(QTreeView *view, const QModelIndex &index)
{
    if(!index.isValid())
        return;
    const QAbstractItemModel *model = view->model();
    if(!model)
        return;
    if(!index.data(MergeFlagRole).toBool() && model->hasChildren(index)) // 合并行保持折叠；模型 rowCount 不受折叠影响，后代仍会被递归到
        view->expand(index);
    const int rows = model->rowCount(index);
    for(int row = 0; row < rows; ++row)
        expandAllExceptMerged(view, model->index(row, kPhenomenonColumn, index));
}

namespace {
// 中键点击某行 → 展开整棵子树（跳过 ⇒ 合并行）并吞掉事件；QAbstractScrollArea 的鼠标事件接收者是 viewport()，故过滤器装在 viewport 上
class MiddleClickExpandFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if(event->type() != QEvent::MouseButtonPress)
            return QObject::eventFilter(watched, event);
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if(mouseEvent->button() != Qt::MiddleButton)
            return QObject::eventFilter(watched, event);
        QTreeView *treeView = qobject_cast<QTreeView *>(watched); // 装 viewport 时 watched 是 viewport（其父即 view）
        if(!treeView)
            treeView = qobject_cast<QTreeView *>(watched->parent());
        if(!treeView)
            return QObject::eventFilter(watched, event);
        const QModelIndex index = treeView->indexAt(mouseEvent->position().toPoint());
        if(!index.isValid())
            return QObject::eventFilter(watched, event);
        expandAllExceptMerged(treeView, index);
        return true;
    }
};
}

void installMiddleClickExpandAllExceptMerged(QTreeView *view)
{
    view->viewport()->installEventFilter(new MiddleClickExpandFilter(view)); // filter 以 view 为 parent 随其释放
}

bool selectKnowledgeRowByDerivedAbs(QTreeView *view, const QString &derivedAbs)
{
    if(derivedAbs.isEmpty())
        return false;
    QStandardItemModel *model = itemModel(view);
    if(!model || model->rowCount() == 0)
        return false;

    const auto hits = model->match(model->index(0, kPhenomenonColumn), DerivedAbsRole,
                                   derivedAbs, 1, Qt::MatchExactly | Qt::MatchRecursive); // 与 questions.abstractName 同一名字空间
    if(hits.isEmpty())
        return false;
    const QModelIndex current = view->currentIndex();
    if(current.isValid() && current.data(DerivedAbsRole).toString() == derivedAbs) // 已在目标行则不重复设置
        return true;

    const QModelIndex target = hits.first();
    for(QModelIndex ancestor = target.parent(); ancestor.isValid(); ancestor = ancestor.parent())
        view->expand(ancestor);
    view->setCurrentIndex(target);
    view->scrollTo(target, QAbstractItemView::EnsureVisible);
    return true;
}

QString questionDerivedAbsForIndex(QTreeView *view, const QModelIndex &index)
{
    if(!index.isValid())
        return {};
    if(index.data(MergeFlagRole).toBool() && view && !view->isExpanded(index)) // 仅折叠态 ⇒ 合并行回退到唯一 is 子行的 DerivedAbs（代表该层嵌套）；展开态按父行 DerivedAbs 查题
    {
        const QAbstractItemModel *model = index.model();
        if(model && model->rowCount(index) == 1)
        {
            const QString childDerivedAbs = model->index(0, kPhenomenonColumn, index).data(DerivedAbsRole).toString();
            if(!childDerivedAbs.isEmpty())
                return childDerivedAbs;
        }
    }
    return index.data(DerivedAbsRole).toString();
}

void forEachTreeItem(QStandardItemModel *model, const std::function<void(QStandardItem *)> &visit)
{
    std::function<void(QStandardItem *)> walk = [&](QStandardItem *item) {
        if(!item)
            return;
        visit(item);
        for(int row = 0; row < item->rowCount(); ++row)
            walk(item->child(row));
    };
    for(int row = 0; row < model->rowCount(); ++row)
        walk(model->item(row));
}

int phenIdOfItem(const QStandardItem *item)
{
    return item->index().siblingAtColumn(kDueColumn).data(PhenIdRole).toInt();
}

// 复习后按最新排期重写"到期"列天数：只改文本，不重建树（展开与选中都不动）
void refreshDueDaysColumn(QTreeView *view)
{
    QStandardItemModel *model = itemModel(view);
    if(!model)
        return;
    refreshQuestionDueDates();
    forEachTreeItem(model, [&](QStandardItem *item) {
        QStandardItem *dueItem = model->itemFromIndex(item->index().siblingAtColumn(kDueColumn));
        if(dueItem)
            dueItem->setText(questionDueDaysText(item->data(DerivedAbsRole).toString()));
    });
}

QSet<int> collectExpandedPhenIds(QTreeView *view)
{
    QSet<int> ids;
    QStandardItemModel *model = itemModel(view);
    if(!model)
        return ids;
    forEachTreeItem(model, [&](QStandardItem *item) {
        if(view->isExpanded(item->index()))
            ids.insert(phenIdOfItem(item));
    });
    return ids;
}

void restoreExpandedPhenIds(QTreeView *view, const QSet<int> &ids)
{
    QStandardItemModel *model = itemModel(view);
    if(!model)
        return;
    forEachTreeItem(model, [&](QStandardItem *item) {
        if(ids.contains(phenIdOfItem(item)))
            view->expand(item->index());
    });
}

QList<QStringList> groupDerivedAbsBySubjectRelation(QTreeView *view, const QStringList &derivedAbsList)
{
    QList<QStringList> groups;
    if(derivedAbsList.isEmpty())
        return groups;

    QHash<QString, int> groupIndexOf; // 键取自名自身：树是 DAG 展开，同名词在多个父行重复出现，按首次出现定主词会拆散兄弟行
    auto placeDerivedAbs = [&](const QString &derivedAbs) {
        QString subject;
        QString relation;
        if(!ToolFunctions::splitDerivedAbsTail(derivedAbs, &subject, &relation))
        {
            subject = derivedAbs; // 非法名：整条自成一组
            relation.clear();
        }
        const QString key = subject + kGroupKeySeparator + relation;
        int index = groupIndexOf.value(key, -1);
        if(index < 0)
        {
            index = groups.size();
            groupIndexOf.insert(key, index);
            groups.append(QStringList{});
        }
        groups[index].append(derivedAbs);
    };

    QSet<QString> placed;
    if(QStandardItemModel *model = itemModel(view))
    {
        forEachTreeItem(model, [&](QStandardItem *item) {
            const QString derivedAbs = item->data(DerivedAbsRole).toString();
            if(derivedAbs.isEmpty() || placed.contains(derivedAbs) || !derivedAbsList.contains(derivedAbs))
                return;
            placed.insert(derivedAbs);
            placeDerivedAbs(derivedAbs); // 按树序归组
        });
    }
    for(const QString &derivedAbs : derivedAbsList) // 未在树中出现的名：按输入顺序接在后面
        if(!placed.contains(derivedAbs))
            placeDerivedAbs(derivedAbs);

    return groups;
}

// ===== 顶层宾词（＝ ctrl+左键 的那个对象）：两处口径都必须与宾词视图同源，绝不手写 world→has:[…] 公式
// （世界层宾语也可能是限定名，拼装会错，见 docs/05 #33）=====

QHash<QString, QStringList> topLevelDerivedAbsByObjectWord()
{
    QHash<QString, QStringList> byWord;
    KnowledgeDB *db = KnowledgeDB::instance();
    for(const Abstract &object : db->findObjects(kHasPredicate, kWorldName))
    {
        const QString word = db->unqualify(object.name);
        if(!word.isEmpty() && !byWord[word].contains(object.name))
            byWord[word].append(object.name);
    }
    return byWord;
}

QString topLevelObjectWordOfDerivedAbs(const QString &derivedAbs)
{
    if(derivedAbs.isEmpty())
        return {};
    KnowledgeDB *db = KnowledgeDB::instance();
    QString topName; // 库内名：取最长匹配（更具体的支）
    for(const Abstract &object : db->findObjects(kHasPredicate, kWorldName))
        if((derivedAbs == object.name || ToolFunctions::isDerivedAbsUnder(derivedAbs, object.name))
           && object.name.size() > topName.size())
            topName = object.name;
    return topName.isEmpty() ? QString() : db->unqualify(topName);
}

TreeNode *fillObjectWordModel(QStandardItemModel *model, const QString &word)
{
    return fillFindObjsModel(model, kWorldName, kHasPredicate, word, true);
}