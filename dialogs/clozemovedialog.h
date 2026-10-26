#ifndef CLOZEMOVEDIALOG_H
#define CLOZEMOVEDIALOG_H

#include <QDialog>
#include <QSet>

namespace Ui {
class ClozeMoveDialog;
}

class QuestionSql;
class QStandardItemModel;
class QSqlRelationalTableModel;
struct TreeNode;

// 将指定答案（cloze）迁移到另一道题：左树选概念、右列其子树题目，选定后由调用方改写两侧 answer.json（同一 cloze id，FSRS 状态保留）。
class ClozeMoveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClozeMoveDialog(QuestionSql *questionSql, QWidget *parent = nullptr);
    ~ClozeMoveDialog();

    // 打开时装配：填完整知识树并预选 sourceDerivedAbs 所在行、列出其子树题目；excludeQId 为源题目自身（从右侧列表滤除）
    void setup(const QString &sourceDerivedAbs, const QSet<int> &expandedPhenIds, int excludeQId);

    int selectedQId() const;          // 未选返回 -1

public slots:
    void accept() override; // 未选中有效目标题目时不允许确定

private slots:
    void on_phenTreeView_clicked(const QModelIndex &index);
    void on_questionsTreeView_doubleClicked(const QModelIndex &index);

private:
    void setupKnowledgeTree();
    void setupQuestionTreeView();
    void loadQuestionsForDerivedAbs(const QString &derivedAbs);
    void configureQuestionColumns();

    Ui::ClozeMoveDialog *ui;
    QuestionSql *questionSql;
    QStandardItemModel *model = nullptr;
    TreeNode *knowledgeRoot = nullptr;              // buildTree 返回树的属主，析构时释放
    QSqlRelationalTableModel *questionTableModel = nullptr;
    int excludeQId = -1;
};

#endif // CLOZEMOVEDIALOG_H