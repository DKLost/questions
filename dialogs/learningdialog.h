#ifndef LEARNINGDIALOG_H
#define LEARNINGDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QJsonArray>
#include <QLayoutItem>
#include <QLineEdit>
#include <QFont>
#include <QFontMetrics>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QLabel>
#include <QSqlTableModel>
#include <QSqlRelationalTableModel>
#include <QRandomGenerator>
#include <QSqlRecord>
#include <QJsonObject>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLayout>
#include <QProcess>
#include <QSet>
#include <QHash>
#include <QPointer>
#include <QTextCursor>
#include <QTextDocument>
#include <QTreeView>
#include "core/questionsql.h"
#include "services/answerstore.h"
#include "services/questionpreview.h"

namespace Ui {
class LearningDialog;
}

class QStandardItemModel;
class QStandardItem;
struct TreeNode;

class LearningDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LearningDialog(QuestionSql *sqlConnection, QWidget *parent = nullptr);
    ~LearningDialog();

    void set_question(int id);
    void set_items_table(QString filter); // 设置复习集过滤
    void clear_question_display();

    void start_timer();
    void stop_timer();

    void setIsSpeedLearn(bool enabled);
    void setOnlyToLearn(bool enabled);
    int getLastId() const;

    // 打开复习前与主界面知识树同步一次：检索条件、视图形态、展开集、当前行 DerivedAbs
    void setKnowledgeViewSync(bool findActive,
                              const QString &subj, const QString &pred, const QString &obj,
                              const QSet<int> &expandedPhenIds, const QString &currentDerivedAbs);

private slots:
    void on_pushButton_clicked();
    void timerHandler();
    void totalTimerHandler();
    void on_LearningDialog_finished(int);
    void on_tableView_clicked(const QModelIndex &index);
    void on_findObjsPushButton_clicked();                   // 知识检索（只读复用主界面 tag 视图）
    void on_phenTreeView_clicked(const QModelIndex &index); // 点知识行跳转到对应题目（与主界面一致）
    void on_comboBox_currentTextChanged(const QString &sortMode);
    void onAnswerInputChanged(const QString &text); // 输入行：自适应宽度 + typst 预览
    void onPoolAnswerChanged(int poolIndex);        // 乱序池选择：自适应大小 + 同池去重
    void onTypstWatcher_standard_output();

private:
    enum class AnswerStage // 本题作答阶段（提交按钮的三分支语义）
    {
        Waiting,      // 未提交
        PreSubmitted, // 已预提交：输入锁定，等待正式提交
        Submitted,    // 已正式提交：再点击即切换下一题
    };

    struct AnswerRowControls // 答案网格一行的控件集合
    {
        QLabel *lineNumber = nullptr;
        QLineEdit *input = nullptr;
        QLabel *typstPreview = nullptr;
        QLabel *answerLabel = nullptr;       // 标准答案（文本或公式图）
        QLabel *answerSourceLabel = nullptr; // typst 行专有：标准答案的原文，显示在输入框正下方便于对照纠错
        QCheckBox *checkBox = nullptr;
        QLabel *timeLabel = nullptr;
        QLabel *goodTimeLabel = nullptr;
        QWidget *ratingBox = nullptr; // manual 行专有：四档自评按钮容器（预提交后显示）

        // 由 answerRowAt 填充：参与作答行的结构（输入行在上、对照行在正下方）
        QHBoxLayout *inputBox = nullptr;  // 输入行：输入框 + typst 实时预览
        QVBoxLayout *compareBox = nullptr; // 纵向容器：[输入行, 对照行]
        QHBoxLayout *belowBox = nullptr;  // 对照行：typst 行含"原文"标签，其后是标准答案
    };

    struct AnswerGridContext // 建网格时记录"需要复习/需要显示"的三类答案
    {
        QSet<int> toLearnPools;
        QSet<int> toLearnInjects;
        QVector<int> notToLearnRows;
    };

    struct ChainCard // 复习集内一道含链卡槽的题
    {
        int questionId = -1;
        int aid = -1;
        QString derivedAbs;
        bool typst = false; // 链卡(typst)：用户答的是宾词的公式体（两端 $ 不敲）
    };

    struct GateRecord // 门控演练的待提交记录（组大提交后清空）
    {
        int questionId = -1;
        int groupIndex = -1;
        int chainAid = -1;
        QString derivedAbs;
        QString input;
        QString rebuilt;
        int timeMs = 0;
        bool typst = false; // 链卡(typst)：归属判定改按公式体全表匹配
    };

    struct GateLanding // 一条记录的归属判定：输入对应的名字该记到谁头上
    {
        bool matched = false;
        int questionId = -1;
        int aid = -1;
    };

    Ui::LearningDialog *ui;

    QTimer *timer; // 当前题作答计时
    QTime answerElapsed;
    QTimer *totalTimer; // 本次复习总计时
    QTime sessionElapsed;
    int totalCount;
    int correctCount;
    bool isSpeedLearn;

    AnswerStage answerStage = AnswerStage::Waiting;
    QFontMetrics *fontMetrics;
    QuestionSql *questionSql;
    AnswerStore *answerStore; // answer.json 读取出口（链卡判定、当前题答案）
    QLabel *answerElapsedLabel;
    QSqlRelationalTableModel *questionTableModel;
    int currentQuestionId;
    QJsonArray currentAnswers;
    int lastAnsweredRowIndex; // 刚提交题目的表行号：作为"提交后再点击"切换下一题的锚点

    int lastReviewedQuestionId;
    bool onlyToLearn;
    int answerRowCount;
    QLineEdit *currentLineEdit;
    QProcess typstWatchProcess{this};

    QStandardItemModel *knowledgeModel = nullptr; // 知识索引区只读模型（与主界面同渲染，仅显示/检索/定位）
    TreeNode *knowledgeRoot = nullptr;            // 当前模型对应的 buildTree 属主（全树或检索树）
    QStandardItemModel *phenObjModel = nullptr;   // 宾词视图模型（反选兜底时按顶层宾词填充，默认空）
    TreeNode *phenObjKnowledgeRoot = nullptr;     // 与之对应的 buildTree 属主

    bool syncFindActive = false; // 主界面知识树同步快照（setKnowledgeViewSync 写入，set_items_table 建树时应用）
    QString syncSubj, syncPred, syncObj;
    QSet<int> syncExpandedPhenIds;
    QString syncCurrentDerivedAbs;

    // 右键预览：共用窗池（一题一窗、同题复用、新窗逐个错开）；手势侧只记"本次手势那个窗"
    QuestionPreview::WindowPool previewWindows{this};
    QPointer<QWidget> dragPreviewWindow;

    QHash<int, bool> chainCardByQId; // 题 id → 是否含 auto(chain) 槽（复习中答案类型不变，会话内缓存）
    QHash<int, QString> manualRatings; // 行号 → 用户在 manual 行选择的评分（缺省 = 按用时反推）
    QHash<int, QPair<bool, bool>> manualCheckBaseline; // 行号 → "未自评时"勾选框的 (checked, enabled)：取消自评按此还原

    QTime totalGoodTime;
    int totalRemainingTimeMsec;

    // ===== 阶段二：分组概念重放（方案见 docs/02） =====
    QHash<int, int> chainAidByQId;                     // 链卡题 id → 链卡 aid（扫描时缓存）
    QString gateBaseFilter;                            // 复习集原始 filter（排除项在其上叠加；再练即重跑它）
    QStringList gateHiddenIds;                         // 被隐藏的子行题 id（禁用并隐藏）
    QSet<int> gateCardIds;                             // 本轮全部可见链卡题 id（复习行总集合）
    QList<ChainCard> gateCards;                        // 可见链卡题（供分组与按名字归属）
    QSet<int> gateAnsweredIds;                         // 已作答离表的题 id（整轮累计）
    QSet<QString> gateLockDerivedAbs;                   // 有子行的链卡行 DerivedAbs：锁定展开（子行不显示）
    QList<QList<int>> gateGroups;                      // 每组 = 同一主词+同一关系段的题 id（组内按树序）
    QHash<int, int> gateGroupOfQuestion;               // 题 id → 所属组下标（缺省 = 未分组）
    QList<GateRecord> gateRecords;                     // 待组提交的记录
    bool gateActive = false;                           // 当前题处于链卡直接作答态
    QLineEdit *gateInput = nullptr;                    // 标题链卡输入框
    int gateChainRow = -1;                             // 链卡行在答案网格中的行号
    QList<int> gateWrongQIds;                          // 整轮累计判错题 id（仅用于收尾提示计数）
    int gatePendingRollGroup = -1;                     // 已提交待回车切换的组（-1 = 无）
    QHash<QString, QPair<QString, int>> gateDrillView; // DerivedAbs→(对象段文本, 状态)：模型重建后恢复现场

    // 初始化 / 网格 / 控件小工具
    void initKnowledgeTree();
    void initSubmitShortcut();
    QGridLayout *answerGrid() const;                  // 答案网格（groupBox 的布局）
    AnswerRowControls answerRowAt(int row) const;     // 取一行的控件集合（行不存在时各控件为空）
    bool rowParticipates(int row) const;              // 该行是否参与作答（未到期的空不参与）
    QHBoxLayout *answerBelowBox(int row) const;       // 输入框正下方的对照行布局（[0]=typst 原文, [1]=标准答案/池选框）
    int answerSlotIndexOf(int row) const;             // 对照行里"标准答案"所占下标（typst 行前有"原文"，故为 1；其余为 0）
    QComboBox *answerComboAt(int row) const;          // 对照行里的乱序选框（非池行返回空）
    void alignCompareLabels(const AnswerRowControls &controls); // 揭示时按输入框内容区左内缩对齐对照标签，使文字/图形逐像素对位
    int focusedAnswerRow() const;                     // 当前焦点输入框的行号（不属于本网格则 -1）
    void resizePoolComboBox(QComboBox *comboBox, int poolIndex);
    void reassignPoolSelections(QComboBox *changedComboBox, int poolIndex);
    QComboBox *poolComboBoxAt(int row, int pool) const; // 取该行属于指定池的选框
    void reloadQuestionTable();                       // select + 取全量（避免各处重复 canFetchMore 样板）
    void updateAccuracyLabel(int batchCorrect, int batchTotal);
    QLabel *summaryCheckLabel() const;                // 底部汇总行的结论标签
    QLabel *summaryGoodTimeLabel() const;             // 底部汇总行的良好时间标签

    // 复习集加载（set_items_table）
    void reloadKnowledgeTreeFromSync(); // 按同步快照重建只读知识树并恢复展开/选中
    void refreshTotalTimeLabels();      // 复习集总时长 / 剩余时长

    // 载题（set_question）
    void resetForNewQuestion(int questionId);
    void selectQuestionDerivedAbsInTree(int questionId);
    void rebuildAnswerGrid();
    AnswerRowControls createAnswerRowWidgets(int rowIndex);
    void applyAnswerStyle(const AnswerRowControls &row, int rowIndex, const QJsonObject &answer, const QString &derivedAbs);
    void applyChainTypstAnswerLabel(QLabel *label, const QString &objectText); // 链卡(typst)标准答案：渲染宾词公式图（未就绪先显公式体）
    void refreshChainTypstAnswerLabels();                                // typst 图落盘后，把链卡(typst)行的原文答案换成公式图
    void placeAnswerRow(const AnswerRowControls &row, int rowIndex, const QJsonObject &answer, AnswerGridContext &context);
    void replacePoolAnswerCells(const QJsonArray &answers);
    void addPoolComboBoxItem(QComboBox *comboBox, const QJsonObject &poolAnswer, int poolRow);
    void showNotToLearnAnswers(const AnswerGridContext &context); // 未到期但需显示的空（池内/注入）
    void appendAnswerSummaryRow(const QJsonArray &answers);
    void appendNotToLearnAnswersToQuestion(const QJsonArray &answers, const QVector<int> &notToLearnRows);
    QString answerImagePath(int answerId, int questionId) const;      // data/<题>/<答案>.png
    void showQuestionPreviewForDerivedAbs(const QString &derivedAbs, bool freshPress); // 该现象的题目页（未到期空位填绿）弹独立预览窗
    bool previewPhenomenonOnRightClick(QObject *watched, QEvent *event); // 知识树右键 → 预览（并避免右键"假选中"）
    void showObjectWordTreeFor(const QString &derivedAbs);               // 反选兜底：以该行的顶层宾词填本窗宾词视图并选中

    // 提交（两段式）
    void preSubmit();
    void submit();
    void revealAnswerRow(int row, const QJsonObject &answer, const QJsonArray &answers); // 锁定输入并揭示该行
    void checkAnswerRow(const AnswerRowControls &row, const QJsonObject &answer);        // auto / manual / auto(chain) 判分并勾选
    void revealPoolAnswerRow(int row, const QJsonObject &answer, const QJsonArray &answers);
    void removeMatchedPoolOption(QComboBox *matchedComboBox, int row, const QJsonObject &answer, const QJsonArray &answers);
    void writeAnswerRowState(int row, const QJsonObject &answer, const QJsonArray &answers, bool &anyRowWrong); // 写 FSRS 并计入正确率（按行独立计对）
    void applyRatingToSummaryRow(bool wrong); // 汇总行结论（困难/简单/良好/出错）
    QWidget *createManualRatingBox(int rowIndex);              // manual 行四档自评按钮
    void showManualRatingRow(int rowIndex);                    // 展示某行四档自评并记下勾选框基线
    void applyManualRating(int rowIndex, int optionIndex, bool selected); // 选中/取消自评并同步勾选框
    void clearManualRating(int rowIndex);                      // 取消自评：按基线还原勾选框
    QString chosenManualRating(int rowIndex) const;
    QString firstChosenManualRating() const; // 本题首个已自评档位（汇总行显示用）
    void leaveAnsweredQuestionRow(bool wrong);
    void advanceToNextQuestion();                                    // 提交后切换下一题
    bool rescheduleAnsweredRowForDefaultOrder();                     // 默认排序：重排刚提交题的 orderNum

    // 复习遮罩
    bool questionHasChainCard(int questionId); // 带缓存
    QSet<QString> collectMaskedDerivedAbs();
    void applyMaskToTreeItem(QStandardItem *item, const QSet<QString> &maskedDerivedAbs, QTreeView *view);
    void applyAnswerMasksTo(QStandardItemModel *model, QTreeView *view, const QSet<QString> &maskedDerivedAbs);
    void refreshAnswerMasks();

    // 门控演练（docs/02）
    QList<ChainCard> scanChainCards();
    QSet<int> hideChildRowsOfCards(const QList<ChainCard> &cards); // 返回被隐藏的子行题 id
    void selectGateReviewRows(const QList<ChainCard> &cards, const QSet<int> &hiddenIds);
    void buildGatePlan();
    void buildGateGroups();
    void rebuildGateFilter();
    void enterGateMode(int questionId);
    void hideNonChainAnswerRows(QGridLayout *layout);
    void keepOnlyFirstQuestionLine(int questionId);
    void gateRecordAnswer();
    bool gateReviewNextRow(int groupIndex);
    bool gateMaybeSubmitGroup(int groupIndex);
    void gateSubmitGroup(int groupIndex);
    QHash<QString, int> gateAnswerOwners(const QList<int> &questionIds) const; // 显示名 → 题 id（归属用）
    QString gateDerivedAbsOfQuestion(int questionId) const;
    QList<GateLanding> resolveGateLandings(const QList<GateRecord> &records, int groupIndex);
    bool writeGateGroupToDatabase(const QList<GateRecord> &records, const QList<GateLanding> &landings);
    void revealGateGroupResults(const QList<GateRecord> &records, const QList<GateLanding> &landings);
    void gateFinishRolling();
    void writeGateRowText(const QString &derivedAbs, const QString &objectText, int drillState);
    void applyDrillStateToItem(QStandardItem *item, const QString &objectText, int drillState);
    void mirrorDrillStateToMergedParent(QStandardItem *item, const QString &objectText, int drillState);
    void setTreeRowText(QStandardItem *item, const QString &base, const QString &full); // ⇒ 合并行的展开/折叠文本
    void applyDrillColor(QStandardItem *item, int drillState);
    int questionIdByDerivedAbsName(const QString &derivedAbs) const; // 全表按 abstractName 精确查题（归属兜底用）
    int questionIdByChainTypstBody(const QString &body) const;       // 全表按宾词公式体查题（链卡(typst) 的归属兜底）
    QStandardItem *findKnowledgeItemByDerivedAbs(const QString &derivedAbs);
    bool isDerivedAbsLocked(const QModelIndex &index) const; // 门控行锁判定（展开锁定守卫用）
    void reapplyGateDrillView();                        // 模型重建（检索）后按快照恢复演练现场
    void restartReview();                               // 再练：重跑本轮复习集（判错题已被 FSRS 排到当天）
    void discardGateCollection();                       // 关闭对话框：未完成记录作废（零写库）并恢复树行

protected:
    bool eventFilter(QObject *watched, QEvent *event);

private:
    bool interceptLockedRowExpand(QObject *watched, QEvent *event); // 门控行展开锁定守卫
    bool mouseOnExpandIndicator(const QModelIndex &index, int x) const;
    bool focusFirstAnswerInputOnTab(QObject *watched, QEvent *event);      // Tab：浏览区 → 首个可作答输入框
    bool focusAnswerRowFromBlankClick(QObject *watched, QEvent *event);    // 题面点空位 → 定位对应答案行
    QLineEdit *firstAnswerInput() const;
    void handleBlankHighlightEvent(QObject *watched, QEvent *event); // 题面空位高亮联动
    void highlight_blank_by_number(int blankNumber);
    void revealBlankNeighbors(const QTextCursor &blankCursor, int blankNumber, bool blankAfterCursor);
    QPoint blankPressPos; // 题面按下位置：用于区分"点击空位"与"拖动选择文本"
    QString rightDragPreviewDerivedAbs; // 右键按住划过时已预览的 DerivedAbs：同一行不重复回填
};

#endif // LEARNINGDIALOG_H