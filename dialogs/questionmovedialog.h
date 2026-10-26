#ifndef QUESTIONMOVEDIALOG_H
#define QUESTIONMOVEDIALOG_H

#include <QDialog>
#include <QSet>

namespace Ui {
class QuestionMoveDialog;
}

class QStandardItemModel;
struct TreeNode;

class QuestionMoveDialog : public QDialog
{
    Q_OBJECT

public:
    // 载入参数：沿用主界面检索条件与展开态；顶层勾选仅现象移动/复制时出现，且只有书写谓词为 has 的行可用（题目移动时隐藏）
    struct Args {
        QString title;
        QString currentDerivedAbs;
        QSet<int> expandedPhenIds;
        QString subj;
        QString pred;
        QString obj;
        bool topLevelVisible = false;
        bool topLevelAllowed = false;
    };

    explicit QuestionMoveDialog(QWidget *parent = nullptr);
    ~QuestionMoveDialog();

    void setup(const Args &args);

    // 确定后返回所选行 DerivedAbs（折叠合并行回退到其 is 子行）；勾选顶层返回 world；未选择返回空串
    QString selectedDerivedAbs() const;

public slots:
    void accept() override; // 未选有效概念且未勾选顶层时不允许确定

private slots:
    void on_phenTreeView_doubleClicked(const QModelIndex &index);
    void on_findObjsPushButton_2_clicked();

private:
    void fillTree(const QString &subj, const QString &pred, const QString &obj);

    Ui::QuestionMoveDialog *ui;
    QStandardItemModel *model = nullptr;
    TreeNode *knowledgeRoot = nullptr;   // 模型对应的 buildTree 属主
    QSet<int> expandedPhenIds;           // 全树载入时恢复展开用
};

#endif // QUESTIONMOVEDIALOG_H
