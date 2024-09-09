#ifndef ANSWEREDITDIALOG_H
#define ANSWEREDITDIALOG_H

#include <QDialog>

namespace Ui {
class AnswerEditDialog;
}

class AnswerEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AnswerEditDialog(QWidget *parent = nullptr);
    ~AnswerEditDialog();

    QString getRetString() const;
    void setRetString(const QString &newRetString);
    void clearTextEdit();
    void setTextEdit(QString text);

    void lineEdit_selectAll();
private slots:
    void on_buttonBox_accepted();

    void on_lineEdit_textChanged(const QString &arg1);

private:
    Ui::AnswerEditDialog *ui;
    QString retString;
};

#endif // ANSWEREDITDIALOG_H
