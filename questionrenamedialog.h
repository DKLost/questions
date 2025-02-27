#ifndef QUESTIONRENAMEDIALOG_H
#define QUESTIONRENAMEDIALOG_H

#include <QDialog>

namespace Ui {
class QuestionRenameDialog;
}

class QuestionRenameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuestionRenameDialog(QWidget *parent = nullptr);
    ~QuestionRenameDialog();


    QString getText() const;
    void setText(const QString &newText);

private slots:
    void on_lineEdit_textChanged(const QString &arg1);

private:
    Ui::QuestionRenameDialog *ui;
    QString text;
};

#endif // QUESTIONRENAMEDIALOG_H
