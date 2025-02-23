#ifndef ANSWEREDITDIALOG_H
#define ANSWEREDITDIALOG_H

#include <QDialog>
#include <QMap>
#include <QFileDialog>

namespace Ui {
class AnswerEditDialog;
}

class AnswerEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AnswerEditDialog(QWidget *parent = nullptr);
    ~AnswerEditDialog();

    void resetEdit();
    void lineEdit_selectAll();

    int getQId() const;
    QString getRetType() const;
    QString getRetContent() const;

    void setQId(int newQId);
    void setRetType(const QString &newRetType);
    void setRetContent(const QString &newRetContent);

    void setType(const QString &Type);
    void setContent(const QString &content);

private slots:
    void on_buttonBox_accepted();

    void on_lineEdit_textChanged(const QString &arg1);

    void on_pushButton_clicked();

private:
    Ui::AnswerEditDialog *ui;
    int qId;
    QString retType;
    QString retContent;
    QMap<QString,QString> type{{"自动检查","auto"},{"手动检查","manual"},{"手动检查(图片)","manual(image)"}};
    QMap<QString,QString> typeName{{"auto","自动检查"},{"manual","手动检查"},{"manual(image)","手动检查(图片)"}};
};

#endif // ANSWEREDITDIALOG_H
