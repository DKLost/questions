#ifndef QUESTIONTAGEDITDIALOG_H
#define QUESTIONTAGEDITDIALOG_H

#include <QDialog>
#include <QSqlTableModel>
#include <QSqlQuery>
#include <QStringListModel>

namespace Ui {
class QuestionTagEditDialog;
}

class QuestionTagEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuestionTagEditDialog(QWidget *parent = nullptr);
    ~QuestionTagEditDialog();
    void set_question(int newId);

private slots:
    void on_addButton_clicked();

    void on_delButton_clicked();

    void on_listView_clicked(const QModelIndex &index);

    void on_listView_activated(const QModelIndex &index);

private:
    Ui::QuestionTagEditDialog *ui;
    int qId;
    QStringListModel *tagListModel;
    QStringList tagList;
};

#endif // QUESTIONTAGEDITDIALOG_H
