#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSet>
#include <QStandardItemModel>
#include <QTreeWidgetItem>
#include <QSqlTableModel>
#include <QSqlRelationalTableModel>
#include <QKeyEvent>
#include <QQueue>
#include <QTextDocument>
#include <QTime>
#include "dialogs/learningdialog.h"
#include "core/questionsql.h"
#include "dialogs/answereditdialog.h"
#include "dialogs/settimedialog.h"
#include "dialogs/htmltableadddialog.h"
#include "utils/toolfunctions.h"
#include "dialogs/questionrenamedialog.h"
#include "dialogs/questionmovedialog.h"
#include "dialogs/clozemovedialog.h"
#include "dialogs/bindanswerdialog.h"
#include "dialogs/descadddialog.h"
#include "dialogs/htmltypstadddialog.h"
#include "services/answerstore.h"
#include "services/questionpreview.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct TreeNode; // 知识树节点（core/knowledgedb.h）
class QTreeView;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    friend class BindAnswerDialog;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void load_answer(int questionId);
    void init_answerTableView();
    void init_answerTreeWidget();

    void init_questionTableView();
    int select_question_by_id(int id);

    // 分类功能已删除：知识索引区取代分类树（表、列、对话框均已移除）

    // knowledge tree（知识索引区，buildTree 批量建树显示）
    void reload_knowledgeTreeView(bool collapseAll = true); // collapseAll=false 时不折叠（供新增后恢复展开）
    QSet<int> collectExpandedPhenIds();                     // 收集当前展开行的现象 id（重建后按 id 恢复）
    void restoreExpandedPhenIds(const QSet<int> &ids);
    void applyFindObjs();                                   // 按主词/谓词输入框执行检索并重建索引区（增删后自动刷新复用）

    // 题目表范围过滤：scopeDerivedAbs 为 DerivedAbs 集合（空=全部）；题目属于范围 ⇔ abstractName 等于某条名或以“该名→”开头，rebuildQuestionFilter 再与名称搜索做 AND
    QStringList scopeDerivedAbs;
    void setQuestionScope(const QStringList &derivedAbsList);
    void rebuildQuestionFilter();
    int findQuestionRowById(int id);                        // 当前（已取全量的）模型中按 id 找行，-1 未命中

    void set_and_jump_to_qId(int questionId);
    void saveCurrentQuestionContent();
    void pushKnowledgeSyncToLearningDialog(); // 打开复习前把知识树检索/展开/选中快照推给复习对话框

protected:
    bool eventFilter(QObject *watched, QEvent *event);
    void closeEvent(QCloseEvent *event) override; // 退出前落盘题面与界面状态


private slots:
    void on_answerAddButton_clicked();
    void on_answerDelButton_clicked();
    void on_clozeMovePushButton_clicked(); // 将选中答案(cloze)移动到其他题目（clozeMoveDialog）
    void on_answerTreeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_answerTreeWidget_itemChanged(QTreeWidgetItem *item, int column);
    void on_answerTreeWidget_itemActivated(QTreeWidgetItem *item, int column);
    void on_answerTreeWidget_itemClicked(QTreeWidgetItem *item, int column);
    void answerItemRowsMoved(int startIndex, int endIndex);
    void on_clozeRecallTodayButton_clicked();   // 提前复习：当前空到期日改为今天
    void on_questionRecallTodayButton_clicked(); // 提前复习：本题目下全部空到期日改为今天
    void on_questionTableRecallTodayButton_clicked(); // 提前复习：当前过滤后表内全部题目的空到期日改为今天（先警告确认）

    void on_questionDirOpenButton_clicked();
    void on_questionAddButton_clicked();
    void on_questionTextEdit_textChanged();
    void on_questionPrevieButton_toggled(bool checked); // 题面预览：填满答案只读显示 / 还原真实题面
    void on_questionTableView_clicked(const QModelIndex &index);
    void on_questionDelButton_clicked();
    void on_questionMoveButton_clicked(); // 将当前题改挂到另一个概念（DerivedAbs）（questionMoveDialog）
    void on_questionTableView_activated(const QModelIndex &index);
    void on_questionTableView_entered(const QModelIndex &index);
    void on_questionResetButton_clicked();

    void on_itemLearnButton_clicked();
    void on_categoryLearnButton_clicked();
    void on_categoryToLearnButton_clicked();
    void on_speedLearnButton_clicked();

    void on_htmlImgAddButton_clicked();
    void on_htmlTableAddButton_clicked();

    void on_phenTreeView_clicked(const QModelIndex &index);
    void on_phenTreeView_customContextMenuRequested(const QPoint &pos); // 右键：强制按概念子树过滤题目表

    void onPhenObjTreeClicked(const QModelIndex &index);                  // 宾词视图点行：与主树同一套载题/过滤
    void onPhenObjTreeCustomContextMenuRequested(const QPoint &pos);      // 宾词视图右键：与主树同一套菜单

    void on_setGoodTimeButton_clicked();

    void on_questionRenameButton_clicked();


    void on_answerTreeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

    void on_questionAutoBindInjectButton_clicked(); // 自动注入绑定上一个答案8/11
    void on_questionAutoGoodTimeButton_clicked();   // 自动设置良好时间8/11

    void on_autoNumberButton_clicked();

    void on_underlineToggleButton_clicked();

    void on_descAddButton_clicked();

    void on_questionSearchButton_clicked();

    void on_idLineEdit_returnPressed();

    void on_lineEdit_returnPressed();

    void on_prevQButton_clicked();

    void on_nextQButton_clicked();

    void on_autoNumberAllButton_clicked();

    void on_htmlTypstAddButton_clicked();

    void on_answerGenAddButton_clicked();

    void on_htmlImgAddButton_2_clicked();

    void on_parserButton_clicked();

    void on_phenAddButton_clicked();

    void on_phenDelButton_clicked();

    void on_phenCleanButton_clicked();    // 🧹 清除孤立概念（与知识树右键同一动作）

    void on_phenEditButton_clicked();     // 改选中行（复用 phenAddDialog 预填本行与唯一 is 子行）

    void on_phenTreeMoveButton_clicked(); // 选中行及其子树改挂到另一概念（复用 questionMoveDialog 选目标概念）

    void on_phenTreeCopyButton_clicked(); // 同上但只新建不删除（原树保留）

    void on_findObjsPushButton_clicked();

private:
    Ui::MainWindow *ui;

    QSqlRelationalTableModel *questionTableModel;
    QSqlTableModel *answerTableModel;
    QStandardItemModel *knowledgeItemModel = nullptr; // 知识索引区模型（buildTree）
    TreeNode *knowledgeRoot = nullptr;                // buildTree 树根，重建/退出时释放
    bool findObjsActive = false;                      // 索引区是否处于检索结果视图（增删后自动重检；全树重建时退出）
    QStandardItemModel *phenObjItemModel = nullptr;   // 宾词视图模型（以「world has 宾词」检索，入口只有 ctrl+左键 / 右键菜单）
    TreeNode *phenObjKnowledgeRoot = nullptr;         // 宾词视图 buildTree 树根
    QString phenObjFilterObject;                      // 宾词视图当前过滤用的宾词（重建时保持）
    QTreeView *lastFocusedPhenView = nullptr;         // 最后聚焦的知识视图：共用的增删改按钮作用于它

    AnswerEditDialog *answerEditDialog;
    LearningDialog *learningDialog;
    SetTimeDialog *setTimeDialog;
    HtmlTableAddDialog *htmlTableAddDialog;
    QuestionRenameDialog questionRenameDialog;
    QuestionMoveDialog questionMoveDialog;
    ClozeMoveDialog *clozeMoveDialog;
    BindAnswerDialog *bindAnswerDialog;
    DescAddDialog *descAddDialog;
    HtmlTypstAddDialog *htmlTypstAddDialog;

    QuestionSql *questionSql;
    AnswerStore *answerStore; // answer.json 与 cloze 生命周期的唯一出口
    QuestionPreview::WindowPool previewWindows{this}; // 弹出式题目预览窗池（ctrl+右键 / 右键菜单「弹出题目预览」）
    QDate currentDate;

    int currentQuestionId = -1;
    int editorQuestionId = -1; // 编辑器内容对应的题目 id（-1 = 呈现区为空）；题面只按它落盘
    bool questionPreviewActive = false;
    QTextDocument *realQuestionDocument = nullptr;    // 预览前的真实题面文档（退出预览时切回；不进 HTML 往返）
    QTextDocument *previewQuestionDocument = nullptr; // 预览文档（真实文档克隆 + 答案回填），退出预览即销毁
    QQueue<int> qIdHistoryQueue;
    int qIdHistoryPos = 0;

    void initFonts();
    void initQuestionDatabase();
    void initQuestionEditor();
    void initDialogs();
    void initKnowledgeTree();
    void initPhenObjTree(); // 宾词视图：默认隐藏，ctrl+左键 / 右键菜单「显示顶层宾词」才填内容并显示
    void connectKnowledgeTreeEntered();
    void initSplitter();

    // 界面状态持久化（ui_state.ini）：检索条件/展开集/题目范围/当前题
    void saveUiState();
    void restoreUiState();

    int findQuestionRowGlobal(int id);          // 当前范围找不到时回全表再找
    void selectQuestionRowAndActivate(int row); // 按可见列 id 定位并载题
    void selectQuestionRowAfterDelete(int row);
    bool confirmDeleteQuestion();
    void clearQuestionEditor();                   // 原子解绑（屏蔽信号）并清空题面
    void setQuestionAreasEnabled(bool enabled);   // 无选中题时禁用（变灰）html/cloze 编辑区，有题时恢复
    void syncQuestionAreasWithTableSelection();   // 过滤/重建题目表后按当前行收口（回选原题 或 回到无题禁用空态）
    void loadQuestionIntoEditor(int questionId);  // 原子换绑：先切落盘 id 再换内容，屏蔽 setHtml 触发的保存

    QTreeWidgetItem *answerRowItem(int row) const;
    void reloadAnswersSelectingNeighbor(int questionId, int row);
    int takeClozeMoveSource(int &row); // 校验移动源（源题目 id + 行号），不合法返回 -1
    void applyAnswerTypeDisplay(QTreeWidgetItem *item, int questionId, const QString &type, int answerId, const QString &content);
    void refreshChainTypstAnswerIcons(); // typst 图落盘后：把链卡(typst)答案从文本换成公式图
    void setClozeDueToday(int answerId);
    void refreshDueDatesAfterChange();                                            // 到期日改动后刷新题目表/知识树到期列/当前空排期表
    void openAnswerEditDialog(int questionId, int oldAnswerId, const QJsonObject &answer, const QJsonArray &siblings);
    void applyAnswerEditResult(int questionId, int row, const QJsonObject &previous, QTreeWidgetItem *item);

    int questionIdAttachedToDerivedAbs(const QString &derivedAbs); // 全表查该 DerivedAbs 是否直接挂题，-1 无
    void refreshKnowledgeTreePreservingExpansion();
    void cleanupOrphanConcepts();  // 清除顶层孤立概念：只为"声明个体"而生、没有任何实际知识内容的 world 级概念
    void refreshAllDueDaysColumns(); // 「到期」列在增删题/改挂/迁移/复习后重写：两个知识视图各来一遍
    QTreeView *activePhenView() const;                // 共用的增删改按钮/跳转作用于最后聚焦的知识视图
    QStandardItemModel *activePhenModel() const;      // 与之对应的模型
    void phenRowActivated(QTreeView *view, const QModelIndex &index); // 点行：载题/按概念子树过滤（两视图共用）
    void phenViewContextMenu(QTreeView *view, const QPoint &pos);     // 右键菜单（两视图共用）
    bool previewQuestionAt(QTreeView *view, const QPoint &globalPos);  // ctrl+右键 / 菜单：为光标所在现象行的题目弹出预览窗；命中行返回 true
    bool handlePhenViewCtrlClick(QTreeView *view, QEvent *event); // ctrl+左键＝以该行宾词建宾词视图
    QString objectDisplayOfIndex(const QModelIndex &index) const;      // 行宾词的显示名（建宾词视图的过滤词）
    void showPhenObjTree(const QString &objectDisplay);               // 填充并显示宾词视图
    void refreshPhenObjTreePreservingExpansion();                     // 按当前宾词重建（保留展开）
    bool selectedPhenRoot(QString &rootSubject, int &keepPhenId) const;
    void expandAndSelectPhenRow(int phenId); // 展开祖先并按现象 id 选中该行（新增后保持父行、改动后定位改动行）
    void applyPhenSegmentEdit(int phenId, const QString &fUnEncoded, const QString &xUnEncoded, bool atFlag); // 按差异回写一段
    QString pickTargetConcept(const QString &currentDerivedAbs, const QString &title,
                              bool allowTopLevel, bool topLevelAllowed); // 复用 questionMoveDialog 选目标概念；取消返回空串
    void repointQuestionsUnderRenamedConcept(const QString &oldDerivedAbs, const QString &newDerivedAbs); // 题目挂载键跟随概念改名
    void clearQuestionViewIfShowingDerivedAbs(const QString &derivedAbs);
    bool confirmDeletePhenomenon(const QString &label);
    void setQuestionScopeFromFindResults();

    QString buildScopeCondition(const QStringList &derivedAbsList) const;
    void applyQuestionFilter(const QString &filter);
    QStringList expandedScopeDerivedAbs() const;  // scope 各行连同其子树逐行并入"顶层宾词支"（见 docs/01 §3.4）

    bool handleQuestionTableKeyPress(QEvent *event);
    bool handlePhenViewKeyPress(QTreeView *view, QEvent *event); // 键盘换行 → 与点击该行相同的载题行为（两视图共用）
    void moveQuestionSelectionBy(int delta);

    void enterQuestionPreview(); // 克隆真实题面文档 → 在克隆上填满全部答案 → 切到克隆只读显示
    void leaveQuestionPreview(); // 切回真实题面文档并恢复编辑（保存/换题/取消勾选都从这里收口）
    bool editTypstImageAt(const QPoint &viewportPos); // 双击 typst 公式图片 → 复用 HtmlTypstAddDialog 改公式
    void insertUnorderedListDescription(QTextCursor cursor, const QString &content);
    void insertOrderedListDescription(QTextCursor cursor, const QString &content);
    void insertHeadingDescription(QTextCursor cursor, const QString &content);
    void insertTypstImage(const QString &filePath);
    void rewriteUnderlinedNumber(QTextCursor &cursor, int value);
    void distributeGoodTime(int questionId, QTime &time);

    void setCurrentQId(int questionId);
    void showNoQuestionForConcept(); // 该概念暂无题目：清空概念呈现区
    void select_knowledgeRowByDerivedAbs(const QString &derivedAbs); // 题目 → 索引区：展开并选中对应概念行
    int autoNumberNext(QTextCursor &cursor);
    void insertImageToQuestion(QTextCharFormat::VerticalAlignment align);
    void refreshAfterLearning(bool fullReload);
};
#endif // MAINWINDOW_H