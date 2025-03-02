#ifndef BINDANSWERDIALOG_H
#define BINDANSWERDIALOG_H

#include <QDialog>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QSqlTableModel>
#include <QSqlRelationalTableModel>
#include <QKeyEvent>
#include <QList>
#include <QListWidget>
#include <QProcess>
#include <QFileDialog>
#include <QFontDialog>
#include <QJsonObject>
#include <QTreeView>
#include <QTableView>
#include "questionsql.h"
#include "toolfunctions.h"


namespace Ui {
class BindAnswerDialog;
}

class BindAnswerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BindAnswerDialog(QuestionSql *mainQuestionSql,QTreeView *mainCategoryTreeView,
                              QTableView *mainTagTableView ,QWidget *parent = nullptr);
    ~BindAnswerDialog();
    //answer
    void init_answerTableView();

    //question
    void init_questionTableView();
    void select_question_by_id(int id);
    void set_quesion_tableHeader();
    void set_answer_tableHeader();

    void setRetAId(int newRetAId);

    int getRetAId() const;

    void initDialog(int qId, int aId);
protected:
    bool eventFilter(QObject *watched, QEvent *event);

private slots:
    void on_answerListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

    //question
    void on_questionTableView_clicked(const QModelIndex &index);
    void on_questionTableView_activated(const QModelIndex &index);
    void on_questionTableView_pressed(const QModelIndex &index);
    void on_questionTableView_entered(const QModelIndex &index);

    //category
    void on_categoryTreeView_clicked(const QModelIndex &index);

    //tag
    void on_tagTableView_activated(const QModelIndex &index);
    void on_tagTableView_clicked(const QModelIndex &index);


    void on_buttonBox_accepted();

private:
    Ui::BindAnswerDialog *ui;

    //main
    QTreeView *mainCategoryTreeView;
    QTableView *mainTagTableView;

    //model
    QSqlRelationalTableModel *questionTableModel;
    QSqlTableModel *answerTableModel;
    QSqlTableModel *tagTableModel;

    //other
    QuestionSql *questionSql;
    QMap<int,QList<QStandardItem*>> categoryItemLists;
    bool is_questionTextEdit_editable = false;

    //ret
    int retAId;
};

#endif // BINDANSWERDIALOG_H
