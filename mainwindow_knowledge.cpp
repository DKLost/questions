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
#include <QPair>
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
#include <functional>

namespace {
const QString kQuestionDerivedAbsAttachQuery = QStringLiteral("SELECT id FROM questions WHERE abstractName = ? LIMIT 1");
const QString kScopeArrow = QStringLiteral("→"); // DerivedAbs 名中的层级箭头（scopeDerivedAbs 前缀判段用）
// 知识树
const QString kRootSubjectWorld = QStringLiteral("world");
const QString kIsPredicate = QStringLiteral("is");
const QString kHasPredicate = QStringLiteral("has");
constexpr int kKnowledgePhenIdColumn = 2;

// SQL LIKE 字面量转义：概念名里可能有 % _ \（真实库里就有 %.2f 这种），作"精准前缀"模式时必须转义；
// 单引号按 SQL 字面量规则转成 ''
QString likeLiteral(QString text)
{
    text.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    text.replace(QLatin1Char('%'), QStringLiteral("\\%"));
    text.replace(QLatin1Char('_'), QStringLiteral("\\_"));
    text.replace(QLatin1Char('\''), QStringLiteral("''"));
    return text;
}

// 行文本 = 关系段前缀 + 宾词词面（appendKnowledgeNode 的写法），故反推即得该行的词面；
// 折叠态 ⇒ 合并行显示的是"关系 ⇒ 个体"，其自身词面在 MergeBaseRole 里
QString objectWordOfIndex(const QModelIndex &index)
{
    const QString prefix = index.data(RelationPrefixRole).toString();
    const QString text = index.data(MergeFlagRole).toBool() ? index.data(MergeBaseRole).toString()
                                                           : index.data(Qt::DisplayRole).toString();
    return text.mid(prefix.size());
}

// 该行是否落在 scope 任何一项的支内（＝该项本身，或该项的下级链）
bool isInScopeSubtree(const QString &derivedAbs, const QStringList &scope)
{
    for(const QString &entry : scope)
        if(derivedAbs == entry || ToolFunctions::isDerivedAbsUnder(derivedAbs, entry))
            return true;
    return false;
}
} // namespace

namespace {
// 段1：主体 —f: x—→ DerivedAbs；段2（m 非空时）：DerivedAbs —r: m—→
QList<LineSegment> buildPhenSegments(PhenAddDialog &dlg)
{
    QList<LineSegment> segments;
    segments.append(LineSegment{KnowledgeDB::encodeName(dlg.getF()), KnowledgeDB::encodeName(dlg.getX()), dlg.getAtFlag1()});
    if(!dlg.getM().isEmpty())
        segments.append(LineSegment{KnowledgeDB::encodeName(dlg.getR()), KnowledgeDB::encodeName(dlg.getM()), dlg.getAtFlag2()});
    return segments;
}
} // namespace

// 共用的增删改按钮/跳转作用于最后聚焦的知识视图；未聚焦过任何知识视图时用主树
QTreeView *MainWindow::activePhenView() const
{
    return lastFocusedPhenView ? lastFocusedPhenView : ui->phenTreeView;
}

QStandardItemModel *MainWindow::activePhenModel() const
{
    return activePhenView() == ui->phenObjTreeView ? phenObjItemModel : knowledgeItemModel;
}

// 智能分流：挂题行→定位载题；纯容器行→按该概念子树过滤题目表，不改题面
void MainWindow::on_phenTreeView_clicked(const QModelIndex &index)
{
    phenRowActivated(ui->phenTreeView, index);
}

void MainWindow::onPhenObjTreeClicked(const QModelIndex &index)
{
    phenRowActivated(ui->phenObjTreeView, index);
}

void MainWindow::phenRowActivated(QTreeView *view, const QModelIndex &index)
{
    // 预览是"模式"：下方 saveCurrentQuestionContent 会先退预览以落盘真实题面，故模式须先记下、各出口恢复
    const bool keepPreview = ui->questionPrevieButton->isChecked();
    const auto restorePreview = [this, keepPreview] { if(keepPreview) ui->questionPrevieButton->setChecked(true); };

    saveCurrentQuestionContent();

    const QString derivedAbs = questionDerivedAbsForIndex(view, index); // 折叠态合并行回退到其唯一 is 子行（题目挂在 is 行上）
    if(derivedAbs.isEmpty())
    {
        showNoQuestionForConcept();
        restorePreview();
        return;
    }

    const int questionId = questionIdAttachedToDerivedAbs(derivedAbs); // 查全表，不受当前过滤范围影响
    if(questionId < 0)
    {
        const QString rowDerivedAbs = index.data(DerivedAbsRole).toString(); // 用行自身 DerivedAbs：折叠合并行也覆盖其 is 子树
        if(!rowDerivedAbs.isEmpty())
            setQuestionScope({rowDerivedAbs});
        restorePreview();
        return;
    }

    if(findQuestionRowById(questionId) < 0) // 不在当前范围则回全表，保证行可见后定位载题
        setQuestionScope({});
    const int row = findQuestionRowById(questionId);
    if(row < 0)
    {
        showNoQuestionForConcept();
        restorePreview();
        return;
    }
    selectQuestionRowAndActivate(row); // 内部读按钮态判断是否续预览（此时按钮已弹起），故末尾统一恢复
    restorePreview();
}

// 行宾词显示名：折叠态 ⇒ 合并行显示的正是本行宾词（与 is 子行同宾语），故取本行现象的宾语即可
QString MainWindow::objectDisplayOfIndex(const QModelIndex &index) const
{
    const int phenId = index.siblingAtColumn(kKnowledgePhenIdColumn).data(PhenIdRole).toInt();
    const Phenomenon row = KnowledgeDB::instance()->getPhenomenon(phenId);
    return KnowledgeDB::instance()->unqualify(row.object.name);
}

// ctrl+左键＝"看宾词"手势：消费事件，不改主树选中、不载题
bool MainWindow::handlePhenViewCtrlClick(QTreeView *view, QEvent *event)
{
    if(event->type() != QEvent::MouseButtonPress)
        return false;
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if(mouseEvent->button() != Qt::LeftButton || !(mouseEvent->modifiers() & Qt::ControlModifier))
        return false;
    const QModelIndex index = view->indexAt(mouseEvent->position().toPoint());
    if(!index.isValid())
        return false;
    showPhenObjTree(objectDisplayOfIndex(index));
    return true;
}

// 宾词视图：等价于把三框填成（world has 宾词）后检索主树，只是入口只有 ctrl+左键与右键菜单
void MainWindow::showPhenObjTree(const QString &objectDisplay)
{
    if(objectDisplay.isEmpty())
        return;
    phenObjFilterObject = objectDisplay;
    refreshPhenObjTreePreservingExpansion();
    ui->phenObjTreeView->setToolTip(QStringLiteral("顶层宾词：%1").arg(objectDisplay));
    ui->phenObjTreeView->setVisible(true);
}

void MainWindow::refreshPhenObjTreePreservingExpansion()
{
    if(phenObjFilterObject.isEmpty())
        return;
    const QSet<int> expandedIds = ::collectExpandedPhenIds(ui->phenObjTreeView);
    delete phenObjKnowledgeRoot;
    // 宾词取自树行本身（非手输），故按显示名精确匹配：只呈现这一个顶层概念，不吃同名字串
    phenObjKnowledgeRoot = fillObjectWordModel(phenObjItemModel, phenObjFilterObject);
    setupKnowledgeTreeView(ui->phenObjTreeView); // 模型重建后 section 重置，需重新隐藏 @ 列
    ui->phenObjTreeView->collapseAll();
    ::restoreExpandedPhenIds(ui->phenObjTreeView, expandedIds);
}

int MainWindow::questionIdAttachedToDerivedAbs(const QString &derivedAbs)
{
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(kQuestionDerivedAbsAttachQuery);
    query.addBindValue(derivedAbs);
    query.exec();
    return query.next() ? query.value(0).toInt() : -1;
}

// ctrl+右键 / 右键菜单「弹出题目预览」：为光标所在现象行的题目弹出独立预览窗（未到期空位填绿，
// 与复习窗同一实现、同一窗池口径）。命中行即算手势生效并返回 true，调用方据此吞掉右键菜单。
bool MainWindow::previewQuestionAt(QTreeView *view, const QPoint &globalPos)
{
    if(!view)
        return false;
    const QModelIndex index = view->indexAt(view->viewport()->mapFromGlobal(globalPos));
    if(!index.isValid())
        return false; // 空白处：不吞，右键菜单照常弹出
    const QString derivedAbs = questionDerivedAbsForIndex(view, index); // 折叠态 ⇒ 合并行回退 is 子行
    const int questionId = questionIdAttachedToDerivedAbs(derivedAbs);
    if(questionId > 0)
        previewWindows.showFor(questionSql, questionId, derivedAbs);
    return true; // 该行没有对应题目时也不弹菜单：ctrl+右键在这里就是"预览手势"
}

// 菜单须按 viewport 坐标弹出
void MainWindow::on_phenTreeView_customContextMenuRequested(const QPoint &pos)
{
    phenViewContextMenu(ui->phenTreeView, pos);
}

void MainWindow::onPhenObjTreeCustomContextMenuRequested(const QPoint &pos)
{
    phenViewContextMenu(ui->phenObjTreeView, pos);
}

void MainWindow::phenViewContextMenu(QTreeView *view, const QPoint &pos)
{
    const QModelIndex index = view->indexAt(pos);

    QMenu menu(this);
    QAction *objAct = menu.addAction(QStringLiteral("显示顶层宾词"));
    objAct->setEnabled(index.isValid());
    QAction *previewAct = menu.addAction(QStringLiteral("弹出题目预览"));
    previewAct->setEnabled(index.isValid()
                           && questionIdAttachedToDerivedAbs(questionDerivedAbsForIndex(view, index)) > 0);
    QAction *filterAct = menu.addAction(QStringLiteral("只显示此概念下的题目（含子级）"));
    filterAct->setEnabled(index.isValid());
    QAction *allAct = menu.addAction(QStringLiteral("显示全部题目"));
    allAct->setEnabled(!scopeDerivedAbs.isEmpty());
    QAction *collapseAct = menu.addAction(QStringLiteral("全部折叠"));
    collapseAct->setEnabled(view->model() && view->model()->rowCount() > 0);
    menu.addSeparator();
    QAction *orphanAct = menu.addAction(QStringLiteral("清除孤立概念（无内容的顶层概念）"));

    QAction *chosen = menu.exec(view->viewport()->mapToGlobal(pos));
    if(chosen == objAct && index.isValid())
    {
        view->setCurrentIndex(index);
        showPhenObjTree(objectDisplayOfIndex(index));
    }
    else if(chosen == previewAct)
    {
        previewQuestionAt(view, view->viewport()->mapToGlobal(pos));
    }
    else if(chosen == filterAct && index.isValid())
    {
        const QString derivedAbs = index.data(DerivedAbsRole).toString();
        if(derivedAbs.isEmpty())
            return;
        view->setCurrentIndex(index); // 高亮选中，后续 ➕ 添加也以该行为父
        setQuestionScope({derivedAbs});
    } else if(chosen == allAct)
    {
        setQuestionScope({});
    } else if(chosen == collapseAct)
    {
        view->collapseAll(); // 全部折叠：⇒ 合并行随 collapsed 信号切回完整文本
    } else if(chosen == orphanAct)
    {
        cleanupOrphanConcepts();
    }
}

// 清除顶层孤立概念：见 KnowledgeDB::orphanTopLevelConceptNames 的判据（只为声明个体而生、无子行、无引用的空壳）。
// 题库在另一个库、图谱判据看不到它，故这里再排除"链上挂了题"的概念：链名本身挂题，或题目挂在它的某个下级。
void MainWindow::cleanupOrphanConcepts()
{
    // 已挂题的链名集合（段边界判定用 = / 前缀，不用 LIKE——链名本身可能含 % _）
    QSet<QString> mountedChains;
    QSqlQuery query(questionSql->getDb());
    if(query.exec(QStringLiteral("SELECT abstractName FROM questions "
                                 "WHERE abstractName IS NOT NULL AND abstractName <> ''")))
        while(query.next())
            mountedChains.insert(query.value(0).toString());

    QList<QString> targets;
    for(const QString &bareName : KnowledgeDB::instance()->orphanTopLevelConceptNames())
    {
        const QString chain = KnowledgeDB::instance()->qualify(bareName);
        bool mounted = false;
        for(const QString &mountedName : mountedChains)
            if(mountedName == chain || mountedName.startsWith(chain + QStringLiteral("→")))
            {
                mounted = true;
                break;
            }
        if(!mounted)
            targets.append(bareName);
    }

    if(targets.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("清除孤立概念"),
                                 QStringLiteral("没有可清除的孤立概念。\n\n"
                                                "（孤立概念 = 只为声明个体而生、既没有下级、也没有被任何一行引用的顶层概念）"));
        return;
    }

    const QStringList preview = targets.mid(0, 20);
    QString detail = preview.join(QStringLiteral("\n"));
    if(targets.count() > preview.count())
        detail += QStringLiteral("\n…（共 %1 个）").arg(targets.count());
    if(QMessageBox::warning(this, QStringLiteral("清除孤立概念"),
                            QStringLiteral("将删除以下 %1 个顶层概念；它们没有下级、也没被任何一行引用：\n\n%2\n\n"
                                           "（挂有题目的概念不会被列入）确定要清除吗?")
                                .arg(targets.count())
                                .arg(detail),
                            QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::No)
        return;

    QSet<QString> removedChains;
    int removed = 0;
    for(const QString &bareName : targets)
        if(KnowledgeDB::instance()->removeOrphanTopLevelConcept(bareName))
        {
            removedChains.insert(KnowledgeDB::instance()->qualify(bareName));
            ++removed;
        }

    // 被清掉的概念若还在题目表范围里，范围条件会永远筛不出东西——顺手从范围里摘掉
    QStringList keptScope;
    for(const QString &scope : scopeDerivedAbs)
        if(!removedChains.contains(scope))
            keptScope.append(scope);
    if(keptScope != scopeDerivedAbs)
        setQuestionScope(keptScope);

    refreshKnowledgeTreePreservingExpansion();
    QMessageBox::information(this, QStringLiteral("清除孤立概念"),
                             QStringLiteral("已清除 %1 个孤立概念。").arg(removed));
}

// 工具条上的 🧹：与知识树右键「清除孤立概念」是同一个动作（同一实现，只是多一个入口）
void MainWindow::on_phenCleanButton_clicked()
{
    cleanupOrphanConcepts();
}

void MainWindow::showNoQuestionForConcept()
{
    clearQuestionEditor();
    ui->answerTreeWidget->clear();
    answerTableModel->setFilter(QStringLiteral("id = -1")); // 当前空排期表：无题时不留上一题的旧行
    answerTableModel->select();
    ui->questionTableView->setCurrentIndex(QModelIndex()); // 无当前题：避免误触发题面保存
    setQuestionAreasEnabled(false);                        // html/cloze 编辑区无题可编：变灰禁用、内容已清空
}

// knowledge tree

void MainWindow::reload_knowledgeTreeView(bool collapseAll)
{
    findObjsActive = false; // 全树重建即退出检索结果视图

    delete knowledgeRoot;
    knowledgeRoot = nullptr;

    if(!knowledgeItemModel)
        knowledgeItemModel = new QStandardItemModel(this);
    knowledgeRoot = fillKnowledgeModel(knowledgeItemModel);

    ui->phenTreeView->setModel(knowledgeItemModel);
    setupKnowledgeTreeView(ui->phenTreeView);

    if(collapseAll)
        ui->phenTreeView->collapseAll(); // 知识索引区默认全部折叠
}

// 重建模型后行对象失效，只能用现象 id 恢复展开
QSet<int> MainWindow::collectExpandedPhenIds()
{
    return ::collectExpandedPhenIds(ui->phenTreeView);
}

void MainWindow::restoreExpandedPhenIds(const QSet<int> &ids)
{
    ::restoreExpandedPhenIds(ui->phenTreeView, ids);
}

// 到期列在增删题/改挂/迁移/复习后重写：两个知识视图各来一遍
void MainWindow::refreshAllDueDaysColumns()
{
    refreshDueDaysColumn(ui->phenTreeView);
    if(ui->phenObjTreeView->isVisible())
        refreshDueDaysColumn(ui->phenObjTreeView);
}

// 主树重建后，宾词视图是同一份数据的另一种过滤，需同步重建（未显示则不动）
void MainWindow::refreshKnowledgeTreePreservingExpansion()
{
    const QSet<int> expandedIds = collectExpandedPhenIds();
    if(findObjsActive)
        applyFindObjs();
    else
        reload_knowledgeTreeView(false);
    restoreExpandedPhenIds(expandedIds);
    if(ui->phenObjTreeView->isVisible())
        refreshPhenObjTreePreservingExpansion();
}

bool MainWindow::confirmDeletePhenomenon(const QString &label)
{
    return QMessageBox::warning(this, QStringLiteral("删除确认"),
                                QStringLiteral("即将删除现象「%1」。\n"
                                               "若该行是 : / @ 行，其派生出的概念（DerivedAbs）下的全部现象（含子行）会一并删除。\n"
                                               "确定要删除吗?")
                                    .arg(label),
                                QMessageBox::Yes | QMessageBox::No)
           != QMessageBox::No;
}

// 关联已随概念删除失效，题目本身保留
void MainWindow::clearQuestionViewIfShowingDerivedAbs(const QString &derivedAbs)
{
    const QModelIndex qIdx = ui->questionTableView->currentIndex();
    if(!derivedAbs.isEmpty() && qIdx.isValid() &&
       qIdx.siblingAtColumn(QuestionAbstractNameColumn).data().toString() == derivedAbs)
    {
        showNoQuestionForConcept();
    }
}

// : / @ 行会连带其派生抽象下的现象一并删除（见 KnowledgeDB::delPhenomenon）
void MainWindow::on_phenDelButton_clicked()
{
    const QModelIndex idx = activePhenView()->currentIndex();
    if(!idx.isValid())
    {
        QMessageBox::information(this, QStringLiteral("删除现象"), QStringLiteral("请先在知识索引区选中一行"));
        return;
    }
    const int phenId = idx.siblingAtColumn(kKnowledgePhenIdColumn).data(PhenIdRole).toInt();
    if(phenId < 0)
        return;
    if(!confirmDeletePhenomenon(idx.siblingAtColumn(0).data().toString()))
        return;

    const QString derivedAbs = idx.data(DerivedAbsRole).toString();
    KnowledgeDB::instance()->delPhenomenon(phenId);
    refreshKnowledgeTreePreservingExpansion(); // 被删的节点自然不在展开集合里
    clearQuestionViewIfShowingDerivedAbs(derivedAbs);
}

// 未选中或无名返回 false，调用方回退 world
bool MainWindow::selectedPhenRoot(QString &rootSubject, int &keepPhenId) const
{
    const QModelIndex idx = activePhenView()->currentIndex();
    if(!idx.isValid())
        return false;
    const QString derivedAbs = idx.data(DerivedAbsRole).toString();
    if(derivedAbs.isEmpty())
        return false;
    rootSubject = derivedAbs;
    keepPhenId = idx.siblingAtColumn(kKnowledgePhenIdColumn).data(PhenIdRole).toInt();
    return true;
}

// 展开祖先并按现象 id 选中该行（新增后保持父行、改动后定位改动行）；作用于当前操作的那个知识视图
void MainWindow::expandAndSelectPhenRow(int phenId)
{
    QTreeView *view = activePhenView();
    QStandardItemModel *model = activePhenModel();
    QTimer::singleShot(0, this, [this, view, model, phenId]() {
        const QModelIndex start = model->index(0, kKnowledgePhenIdColumn);
        const auto hits = model->match(start, PhenIdRole, phenId, 1,
                                       Qt::MatchExactly | Qt::MatchRecursive);
        if(hits.isEmpty())
            return;
        const QModelIndex target = hits.first().siblingAtColumn(0);
        view->expand(target);
        QModelIndex parent = target.parent();
        while(parent.isValid())
        {
            view->expand(parent);
            parent = parent.parent();
        }
        view->setCurrentIndex(target);
        view->scrollTo(target, QAbstractItemView::EnsureVisible);
        view->setFocus(); // 仅 setCurrentIndex 不足以高亮，需强制焦点+selectionModel 选中
        view->selectionModel()->select(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    });
}

void MainWindow::on_phenAddButton_clicked()
{
    QString rootSubject = kRootSubjectWorld;
    int keepPhenId = -1; // 添加前选中的行，添加完成后仍保持选中，便于继续在同一行下添加子行
    const bool hasSelection = selectedPhenRoot(rootSubject, keepPhenId);

    PhenAddDialog dlg(this);
    // 未选中任何节点时，默认勾选"作为顶层概念"且不可取消（world 下仅允许 has 连接）
    if(!hasSelection)
        dlg.setTopLevel(true, true);
    if(dlg.exec() != QDialog::Accepted)
        return;
    if(dlg.isTopLevel())
        rootSubject = kRootSubjectWorld;

    KnowledgeDB::instance()->createLine(rootSubject, buildPhenSegments(dlg));
    refreshKnowledgeTreePreservingExpansion(); // 检索视图下重检；否则不折叠重建以便恢复展开
    if(keepPhenId > 0)
        expandAndSelectPhenRow(keepPhenId);
}

// 编辑选中行：复用 PhenAddDialog 预填（f/x/:@ 取本行；r/m/:@ 取唯一 is 子行，即树上显示为「⇒ 个体」的那段），确定后按差异回写
void MainWindow::on_phenEditButton_clicked()
{
    const QModelIndex idx = activePhenView()->currentIndex();
    if(!idx.isValid())
    {
        QMessageBox::information(this, QStringLiteral("编辑现象"), QStringLiteral("请先在知识索引区选中一行"));
        return;
    }
    const int phenId = idx.siblingAtColumn(kKnowledgePhenIdColumn).data(PhenIdRole).toInt();
    if(phenId < 0)
        return;

    KnowledgeDB *db = KnowledgeDB::instance();
    const Phenomenon p = db->getPhenomenon(phenId);
    if(p.predicate.name.isEmpty())
        return;

    // 第二段＝本行的唯一 is 子行（与树的 ⇒ 合并同条件）；其余情况 r/m 留空，确定后也不动第二段
    int childPhenId = -1;
    QString childPred, childObj;
    bool childAtFlag = false;
    QStandardItemModel *model = activePhenModel();
    if(model->rowCount(idx) == 1)
    {
        const QModelIndex childIdx = model->index(0, kKnowledgePhenIdColumn, idx);
        const int candidateId = childIdx.data(PhenIdRole).toInt();
        const Phenomenon child = db->getPhenomenon(candidateId);
        if(candidateId > 0 && db->barePredicateOf(child).name == kIsPredicate)
        {
            childPhenId = candidateId;
            childPred = kIsPredicate;
            childObj = db->unqualify(child.object.name);
            childAtFlag = db->isLExistAbsPred(child.predicate);
        }
    }

    const QString oldDerivedAbs = idx.data(DerivedAbsRole).toString();
    PhenAddDialog dlg(this);
    dlg.setup(db->unqualify(db->barePredicateOf(p).name), db->unqualify(p.object.name), db->isLExistAbsPred(p.predicate),
              childPred, childObj, childAtFlag, p.subject.name == kRootSubjectWorld);
    if(dlg.exec() != QDialog::Accepted)
        return;

    applyPhenSegmentEdit(phenId, dlg.getF(), dlg.getX(), dlg.getAtFlag1());
    if(childPhenId > 0 && !dlg.getM().isEmpty())
        applyPhenSegmentEdit(childPhenId, dlg.getR(), dlg.getM(), dlg.getAtFlag2());
    else if(childPhenId < 0 && !dlg.getM().isEmpty()) // 本行原无第二段：按录入同口径补一条子行
        db->createLine(db->getDerivedAbs(db->getPhenomenon(phenId)).name,
                       {LineSegment{KnowledgeDB::encodeName(dlg.getR()), KnowledgeDB::encodeName(dlg.getM()), dlg.getAtFlag2()}});

    repointQuestionsUnderRenamedConcept(oldDerivedAbs, db->getDerivedAbs(db->getPhenomenon(phenId)).name);
    refreshKnowledgeTreePreservingExpansion();
    rebuildQuestionFilter(); // 概念名可能已改：题目表按最新 abstractName 重取
    expandAndSelectPhenRow(phenId);
}

// 按差异回写一段：未改动的一段沿用库内形态，改动的一段按显示名重新装配后交给 core 一次改完；装配结果与现状相同时由 core 直通、不写库
void MainWindow::applyPhenSegmentEdit(int phenId, const QString &fUnEncoded, const QString &xUnEncoded, bool atFlag)
{
    KnowledgeDB *db = KnowledgeDB::instance();
    const Phenomenon row = db->getPhenomenon(phenId);
    if(row.predicate.name.isEmpty())
        return;

    const Abstract bare{KnowledgeDB::encodeName(fUnEncoded)};
    db->ensureAbstract(bare);
    // 谓词要与 (∃) 形式一起判"没动"：只翻 : / @ 而书写谓词没变时，仍得按新形态重装
    const bool predKept = fUnEncoded == db->unqualify(db->barePredicateOf(row).name)
                          && atFlag == db->isLExistAbsPred(row.predicate);
    const Abstract predicate = predKept ? row.predicate
                                        : (atFlag ? db->lExistAbsPredicate(bare) : db->rExistAbsPredicate(bare));
    const QString objectUnEncoded = KnowledgeDB::encodeName(xUnEncoded);
    // 宾语没改动则沿用库内形态：unqualify 只剥一层 world→has:[…] 壳，宾语本身是链名（树上的逆行行）时显示名与库内名不同构，重装配会把链名变成叶子名
    const QString object = (xUnEncoded == db->unqualify(row.object.name))
                               ? row.object.name
                               : ((row.subject.name == kRootSubjectWorld) ? objectUnEncoded : db->qualify(objectUnEncoded));
    if(object != row.object.name)
        db->ensureDeclaration(objectUnEncoded);
    db->changePhenomenon(phenId, Phenomenon{predicate, row.subject, Abstract{object}});
}

// 移动现象树：选中行及其整族改挂到另一概念下（改主语 → 派生名整族换名，现象 id 保留），题目挂载键跟随
void MainWindow::on_phenTreeMoveButton_clicked()
{
    const QModelIndex idx = activePhenView()->currentIndex();
    if(!idx.isValid())
    {
        QMessageBox::information(this, QStringLiteral("移动现象树"), QStringLiteral("请先在知识索引区选中一行"));
        return;
    }
    const int phenId = idx.siblingAtColumn(kKnowledgePhenIdColumn).data(PhenIdRole).toInt();
    const QString oldDerivedAbs = idx.data(DerivedAbsRole).toString();
    if(phenId < 0 || oldDerivedAbs.isEmpty())
        return;

    KnowledgeDB *db = KnowledgeDB::instance();
    const bool topLevelAllowed = db->barePredicateOf(db->getPhenomenon(phenId)).name == kHasPredicate; // 只有 has 行可成为顶层
    const QString target = pickTargetConcept(oldDerivedAbs, QStringLiteral("移动现象树"), true, topLevelAllowed);
    if(target.isEmpty() || target == oldDerivedAbs)
        return; // 预选即当前挂载点：直接确定＝不动
    if(target.startsWith(oldDerivedAbs + kScopeArrow)) // 移进自身子树会成环，建树只能退化为叶边
    {
        QMessageBox::information(this, QStringLiteral("移动现象树"), QStringLiteral("不能移动到自身或自身的子概念下"));
        return;
    }

    if(!db->changePhenomenonSubj(phenId, target))
        return;
    repointQuestionsUnderRenamedConcept(oldDerivedAbs, db->getDerivedAbs(db->getPhenomenon(phenId)).name);
    refreshKnowledgeTreePreservingExpansion();
    rebuildQuestionFilter();
    expandAndSelectPhenRow(phenId); // 已挂到新父行下：展开到新位置并保持选中
}

// 复制现象树：选择流程同上，但只新建不删除（原树原样保留，副本不带走题目）
void MainWindow::on_phenTreeCopyButton_clicked()
{
    const QModelIndex idx = activePhenView()->currentIndex();
    if(!idx.isValid())
    {
        QMessageBox::information(this, QStringLiteral("复制现象树"), QStringLiteral("请先在知识索引区选中一行"));
        return;
    }
    const int phenId = idx.siblingAtColumn(kKnowledgePhenIdColumn).data(PhenIdRole).toInt();
    const QString oldDerivedAbs = idx.data(DerivedAbsRole).toString();
    if(phenId < 0 || oldDerivedAbs.isEmpty())
        return;

    KnowledgeDB *db = KnowledgeDB::instance();
    const bool topLevelAllowed = db->barePredicateOf(db->getPhenomenon(phenId)).name == kHasPredicate; // 只有 has 行可成为顶层
    const QString target = pickTargetConcept(oldDerivedAbs, QStringLiteral("复制现象树"), true, topLevelAllowed);
    if(target.isEmpty() || target == oldDerivedAbs)
        return;

    db->copyPhenomenonSubj(phenId, target);
    refreshKnowledgeTreePreservingExpansion();
    expandAndSelectPhenRow(phenId); // 保持选中原行：副本挂在新概念下
}

// 复用 questionMoveDialog 选目标概念（沿用展开态；主界面处于检索态时同步三框条件）；取消返回空串
QString MainWindow::pickTargetConcept(const QString &currentDerivedAbs, const QString &title,
                                      bool allowTopLevel, bool topLevelAllowed)
{
    QuestionMoveDialog::Args args;
    args.title = title;
    args.currentDerivedAbs = currentDerivedAbs;
    args.expandedPhenIds = collectExpandedPhenIds();
    if(findObjsActive) // 过滤方式与主界面同步：检索态带三框条件，否则全树
    {
        args.subj = ui->subjLineEdit->text();
        args.pred = ui->predLineEdit->text();
        args.obj = ui->objLineEdit->text();
    }
    args.topLevelVisible = allowTopLevel;
    args.topLevelAllowed = topLevelAllowed;
    questionMoveDialog.setup(args);
    if(questionMoveDialog.exec() != QDialog::Accepted)
        return {};
    return questionMoveDialog.selectedDerivedAbs();
}

namespace {
// 挂载键里旧链名出现的两处结构位：开头（子行链名「旧→…」）或 [ ] 槽内（@ 行把父链名当主语/宾语内嵌）；槽外的巧合子串不动
bool isStructuralNameOccurrence(const QString &key, int pos, int len)
{
    const bool leftOk = (pos == 0 || key.at(pos - 1) == QLatin1Char('['));
    const bool rightOk = (pos + len == key.size() || key.at(pos + len) == QLatin1Char(']')
                          || key.at(pos + len) == kScopeArrow.at(0));
    return leftOk && rightOk;
}

QString renameDerivedAbsInKey(const QString &key, const QString &oldAbs, const QString &newAbs)
{
    QString renamed = key;
    for(int pos = renamed.indexOf(oldAbs); pos >= 0; pos = renamed.indexOf(oldAbs, pos + newAbs.size()))
    {
        if(isStructuralNameOccurrence(renamed, pos, oldAbs.size()))
            renamed = renamed.left(pos) + newAbs + renamed.mid(pos + oldAbs.size());
    }
    return renamed;
}
} // namespace

// 概念改名/移动后把挂在该链及其各级子链上的题目跟着改挂：名字是结构键，不跟随就与树脱钩
// @ 行的派生名把父链名内嵌在 [ ] 槽里（`宾语→逆谓词:[父链名]`），不走「父链名→」前缀，故按结构位改写而非前缀匹配
void MainWindow::repointQuestionsUnderRenamedConcept(const QString &oldDerivedAbs, const QString &newDerivedAbs)
{
    if(oldDerivedAbs.isEmpty() || newDerivedAbs.isEmpty() || oldDerivedAbs == newDerivedAbs)
        return;

    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.exec(QStringLiteral("SELECT id, abstractName FROM questions WHERE abstractName IS NOT NULL"));
    QList<QPair<int, QString>> hits;
    while(query.next())
    {
        const QString name = query.value(1).toString();
        const QString renamed = renameDerivedAbsInKey(name, oldDerivedAbs, newDerivedAbs);
        if(renamed != name)
            hits.append({query.value(0).toInt(), renamed});
    }

    for(const QPair<int, QString> &hit : hits)
    {
        QSqlQuery update(QSqlDatabase::database(kConnectionName));
        update.prepare(QStringLiteral("UPDATE questions SET abstractName = ? WHERE id = ?"));
        update.addBindValue(hit.second);
        update.addBindValue(hit.first);
        update.exec();
    }
}

// 复习对话框共用同一渲染实现；检索视图下增删现象后重入本函数刷新
void MainWindow::applyFindObjs()
{
    delete knowledgeRoot;
    knowledgeRoot = fillFindObjsModel(knowledgeItemModel,
                                      ui->subjLineEdit->text(), ui->predLineEdit->text(),
                                      ui->objLineEdit->text());
    setupKnowledgeTreeView(ui->phenTreeView);
    ui->phenTreeView->collapseAll(); // 检索结果默认折叠
    findObjsActive = true;
    setQuestionScopeFromFindResults();
}

// 题目在树中 ⇔ abstractName 等于某顶层行 DerivedAbs 或以“其→”开头；三框全空 = 清除范围
void MainWindow::setQuestionScopeFromFindResults()
{
    const bool emptyQuery = ui->subjLineEdit->text().trimmed().isEmpty()
                            && ui->predLineEdit->text().trimmed().isEmpty()
                            && ui->objLineEdit->text().trimmed().isEmpty();
    if(emptyQuery)
    {
        setQuestionScope({});
        return;
    }

    QStringList roots;
    for(int row = 0; row < knowledgeItemModel->rowCount(); ++row)
    {
        const QString derivedAbs = knowledgeItemModel->item(row, 0)->data(DerivedAbsRole).toString();
        if(!derivedAbs.isEmpty())
            roots.append(derivedAbs);
    }
    setQuestionScope(roots);
}

void MainWindow::rebuildQuestionFilter()
{
    QStringList conds;

    const QString name = ui->lineEdit->text().trimmed();
    if(!name.isEmpty())
        conds << QString("questions.name LIKE '%%1%'").arg(QString(name).replace('\'', "''"));

    if(!scopeDerivedAbs.isEmpty())
        conds << buildScopeCondition(expandedScopeDerivedAbs());

    applyQuestionFilter(conds.join(" AND "));
}

// 顶层宾词表与"词面反查"都在 knowledgetreehelper（topLevelDerivedAbsByObjectWord /
// topLevelObjectWordOfDerivedAbs）：口径与宾词视图同源，不手写名字公式

// 题目范围扩展：把 scope 里的每一行**连同其整棵子树**逐行取"宾词词面"，再把该词面作为顶层宾词时的
// 那一支（含子级）并进范围——等价于"对每一行 ctrl+左键 → 显示此概念下的全部题目"（docs/01 §3.4）。
// 只扩"范围"这一层，scopeDerivedAbs 本身仍是用户意图（ini 持久化 / 孤立概念清理都不受影响）。
QStringList MainWindow::expandedScopeDerivedAbs() const
{
    if(scopeDerivedAbs.isEmpty())
        return {};
    auto *model = qobject_cast<QStandardItemModel *>(ui->phenTreeView->model());
    if(!model)
        return scopeDerivedAbs;

    const QHash<QString, QStringList> topByWord = topLevelDerivedAbsByObjectWord();
    QStringList expanded = scopeDerivedAbs;
    QSet<QString> seen;
    for(const QString &entry : expanded)
        seen.insert(entry);

    std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
        for(int row = 0; row < model->rowCount(parent); ++row)
        {
            const QModelIndex index = model->index(row, 0, parent);
            const QString derivedAbs = index.data(DerivedAbsRole).toString();
            if(!derivedAbs.isEmpty() && isInScopeSubtree(derivedAbs, scopeDerivedAbs))
                for(const QString &topDerivedAbs : topByWord.value(objectWordOfIndex(index)))
                    if(!seen.contains(topDerivedAbs))
                    {
                        seen.insert(topDerivedAbs);
                        expanded.append(topDerivedAbs);
                    }
            walk(index);
        }
    };
    walk(QModelIndex());
    return expanded;
}

// 范围条件＝"题挂在该行或该行的下级"（`= C` 或 `C→…`，段边界精确）：题只属于它自己所在的支，
// 不该因为"别的支里把 C 当宾语嵌进更深段"而被收到 C 名下（那是那些题自己的支）。
// 三框检索过滤**现象行**时宾词才用子串（fillFindObjsModel 的 objExactMatch=false，见 02 §4.3）。
QString MainWindow::buildScopeCondition(const QStringList &derivedAbsList) const
{
    const QString column = kAbstractNameField;
    QStringList ors;
    for(const QString &derivedAbs : derivedAbsList)
    {
        const QString literal = likeLiteral(derivedAbs); // 已转义，可直接嵌入单引号字面量
        ors << QStringLiteral("(") + column + QStringLiteral(" = '") + literal
                   + QStringLiteral("' OR ") + column + QStringLiteral(" LIKE '") + literal
                   + kScopeArrow + QStringLiteral("%' ESCAPE '\\')");
    }
    return "(" + ors.join(" OR ") + ")";
}

// 原生 SQL 增删后模型不自动感知；findQuestionRowById 依赖已取全量
void MainWindow::applyQuestionFilter(const QString &filter)
{
    questionTableModel->setFilter(filter);
    questionTableModel->select();
    ToolFunctions::fetchAllRows(questionTableModel);
    syncQuestionAreasWithTableSelection();
}

// 过滤题目表会重建模型、当前行连同选中一起失效（QSqlTableModel::select 走 reset）。
// 此时按规则收口，避免"题面还在、却没有任何选中题目"（答案加/删/改都在 currentIndex 守卫处静默失效）：
//   原当前题仍在筛选结果里 → 回选并保持题面/答案；已不在（如点了无题但子行有题的现象行）→ 呈现区回到禁用空态
void MainWindow::syncQuestionAreasWithTableSelection()
{
    if(ui->questionTableView->currentIndex().isValid())
        return;
    const int row = currentQuestionId > 0 ? findQuestionRowById(currentQuestionId) : -1;
    if(row >= 0)
        selectQuestionRowAndActivate(row);
    else
        showNoQuestionForConcept();
}

void MainWindow::setQuestionScope(const QStringList &derivedAbsList)
{
    scopeDerivedAbs = derivedAbsList;
    rebuildQuestionFilter();
}

int MainWindow::findQuestionRowById(int id)
{
    return ToolFunctions::rowOfValue(questionTableModel, id, QuestionIdColumn);
}

void MainWindow::on_findObjsPushButton_clicked()
{
    applyFindObjs();
}
