#ifndef BINDANSWERDIALOG_H
#define BINDANSWERDIALOG_H

#include <QDialog>
#include <QEvent>
#include <QListWidget>
#include <QSqlRelationalTableModel>
#include <QSqlTableModel>
#include <QTableView>
#include <QTreeView>
#include "core/questionsql.h"
#include "widgets/questiontableviewhelper.h"


namespace Ui {
class BindAnswerDialog;
}

class BindAnswerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BindAnswerDialog(QuestionSql *mainQuestionSql,
                              QTreeView *mainCategoryTreeView,
                              QTableView *mainQuestionTableView,
                              QWidget *parent = nullptr);
    ~BindAnswerDialog();
    void init_answerTableView();

    void init_questionTableView();
    void select_question_by_id(int id);

    void setRetAId(int newRetAId);

    int getRetAId() const;

    void initDialog(int questionId, int answerId);

    void setTreeModel(QAbstractItemModel *model); // 主树可切知识模型，这里显式固定分类模型

    void disableCategoryTree(); // 分类功能已移除，保留 ui 但不可交互

protected:
    bool eventFilter(QObject *watched, QEvent *event);

private slots:
    void on_answerListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

    void on_questionTableView_clicked(const QModelIndex &index);
    void on_questionTableView_activated(const QModelIndex &index);
    void on_questionTableView_entered(const QModelIndex &index);

    void on_categoryTreeView_clicked(const QModelIndex &index);

    void on_buttonBox_accepted();

private:
    void moveQuestionSelection(int rowDelta);

    Ui::BindAnswerDialog *ui;

    QTableView *mainQuestionTableView;

    QSqlRelationalTableModel *questionTableModel;
    QSqlTableModel *answerTableModel;

    QuestionSql *questionSql;

    int selectedAnswerId;
};

#endif // BINDANSWERDIALOG_H