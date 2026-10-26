#ifndef KNOWLEDGETREEHELPER_H
#define KNOWLEDGETREEHELPER_H

#include <QHash>
#include <QSet>
#include <QStandardItemModel>
#include <QTreeView>
#include "core/knowledgedb.h"

// 知识树（KnowledgeDB::buildTree 批量建树）在主界面与复习对话框间共用的渲染/联动逻辑；模型三列：现象 / @ / 到期（天数）。

// ⇒ 合并行自定义角色：折叠态显示“关系 ⇒ 个体”，展开态显示“关系”（子行 is : 个体 显现）
constexpr int MergeFlagRole = Qt::UserRole + 10;       // 本行是否为 ⇒ 合并行
constexpr int MergeBaseRole = Qt::UserRole + 11;       // 展开态文本（仅关系段）
constexpr int MergeFullRole = Qt::UserRole + 12;       // 折叠态文本（关系 ⇒ 个体）
constexpr int DerivedAbsRole = Qt::UserRole + 13;       // 本行承载现象派生的抽象 = DerivedAbs（createLine 的 rootSubject，见 docs/01 定义 1.14b）
constexpr int RelationPrefixRole = Qt::UserRole + 14;  // 行文本中宾词前的关系段（"is : "/"@ "/""，复习遮罩时保留它）
constexpr int PhenIdRole = Qt::UserRole + 15;          // 现象 id（第 3 列显示到期天数，id 只存角色供展开集持久化）

// 清空并用 world 全树填充 model（world 根不入模，其孩子直接挂到模型根）；返回新建的 world 根 TreeNode，调用方负责 delete
TreeNode *fillKnowledgeModel(QStandardItemModel *model);

// 知识检索（tag 视图）：主词空按 world、谓词空按 has，非 world 主体 qualify 且谓词取 rEAbs（只命中 : 行）；
// obj 非空时按**显示名**过滤（objExactMatch=true 取精确相等，否则 LIKE '%obj%' 子串、大小写不敏感）；
// 命中概念按其树中位置保留（祖先补父行、命中处以下整棵展开、其余剪枝）；返回 world 根，调用方删除
TreeNode *fillFindObjsModel(QStandardItemModel *model, const QString &subj, const QString &pred,
                            const QString &obj = QString(), bool objExactMatch = false);

// 递归渲染单个 TreeNode 到 parent 行下（行文本规则、⇒ 合并、DerivedAbsRole 均在此写入）；
// kept 非空时只渲染集合内的子行（检索剪枝用，见 fillFindObjsModel）
void appendKnowledgeNode(TreeNode *node, QStandardItem *parent, const QSet<const TreeNode *> *kept = nullptr);

// 知识树视图统一外观：隐藏 @ 列、表头缩放策略
void setupKnowledgeTreeView(QTreeView *view);

// 只读知识树一体化安装：外观 + ⇒ 合并行切换 + 中键展开整棵子树 + 只读/整行单选/无右键菜单
void installReadOnlyKnowledgeTree(QTreeView *view);

// ⇒ 合并行随展开/折叠切换显示文本
void installMergeToggle(QTreeView *view);

// 离开 ⇒ 合并行唯一的 is 子行时自动折叠其父行；新选择仍在该合并子树内则不折叠；须在 setModel 之后安装
void installCollapseMergeOnLeave(QTreeView *view);

// 鼠标中键点击某行：递归展开该行整棵子树但跳过 ⇒ 合并行（保持折叠）
void expandAllExceptMerged(QTreeView *view, const QModelIndex &index);

void installMiddleClickExpandAllExceptMerged(QTreeView *view);

// 题目 → 知识树：按 DerivedAbs 全等递归匹配 DerivedAbsRole，展开全部祖先并选中滚动到该行；已在目标行或未命中则不改动当前选择，返回是否命中
bool selectKnowledgeRowByDerivedAbs(QTreeView *view, const QString &derivedAbs);

// 树行 → 题目：普通行返回自身 DerivedAbsRole；折叠态 ⇒ 合并行回退到唯一 is 子行的 DerivedAbs
QString questionDerivedAbsForIndex(QTreeView *view, const QModelIndex &index);

// 展开状态按现象 id 收集/恢复（重建模型后行对象失效，只能用第 3 列 phenId 对应）
QSet<int> collectExpandedPhenIds(QTreeView *view);
void restoreExpandedPhenIds(QTreeView *view, const QSet<int> &ids);

// 按最新题目排期刷新"到期"列天数（不重建树，保留展开与选中；复习结束后调用）
void refreshDueDaysColumn(QTreeView *view);

// 按"同一主词 + 同一关系段"把 DerivedAbs 分组（分组键取自 DerivedAbs 名自身，树只决定组与组内顺序；未在树中命中的名按输入顺序排最后）
QList<QStringList> groupDerivedAbsBySubjectRelation(QTreeView *view, const QStringList &derivedAbsList);

// ===== 顶层宾词（＝ ctrl+左键 的那个对象；docs/01 §3.4 的"范围第二段"）=====

// 宾词词面（显示名）→ 顶层宾词行的 DerivedAbs（可能多值：裸名/限定名孪生）；
// 口径与宾词视图完全一致（world has 词面），不手写名字公式，对不上的项自然匹配不到题
QHash<QString, QStringList> topLevelDerivedAbsByObjectWord();

// 该现象行所属"顶层宾词"的词面（指向它的最长的 world→has:[…] 名）；找不到返回空串
QString topLevelObjectWordOfDerivedAbs(const QString &derivedAbs);

// 以某词面作顶层宾词填充 model（等价于 ctrl+左键；精准匹配）；返回树属主，调用方负责 delete
TreeNode *fillObjectWordModel(QStandardItemModel *model, const QString &word);

#endif // KNOWLEDGETREEHELPER_H