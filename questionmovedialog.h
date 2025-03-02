#ifndef QUESTIONMOVEDIALOG_H
#define QUESTIONMOVEDIALOG_H

#include <QDialog>
#include <QStandardItemModel>
#include <QTreeView>

namespace Ui {
class QuestionMoveDialog;
}

class QuestionMoveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuestionMoveDialog(QTreeView *categoryTreeView,QWidget *parent = nullptr);
    ~QuestionMoveDialog();

    int getRetId() const;
    void setRetId(int newRetId);
    void setCurrentIndex(const QModelIndex &index);

private slots:
    void on_buttonBox_accepted();

private:
    Ui::QuestionMoveDialog *ui;
    int retId;
};

#endif // QUESTIONMOVEDIALOG_H
