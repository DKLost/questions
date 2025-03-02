#ifndef ANSWEREDITDIALOG_H
#define ANSWEREDITDIALOG_H

#include <QDialog>
#include <QMap>
#include <QFileDialog>
#include "bindanswerdialog.h"

namespace Ui {
class AnswerEditDialog;
}

class AnswerEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AnswerEditDialog(BindAnswerDialog *mainBindAnswerDialog,QWidget *parent = nullptr);
    ~AnswerEditDialog();

    void resetEdit();
    void lineEdit_selectAll();

    //getter
    int getQId() const;
    QString getRetType() const;
    QString getRetContent() const;
    QTime getRetGoodTime() const;
    int getRetAId() const;


    //setter
    void setQId(int newQId);
    void setRetType(const QString &newRetType);
    void setRetContent(const QString &newRetContent);
    void setRetGoodTime(const QTime &newRetGoodTime);
    void setType(const QString &Type);
    void setContent(const QString &content);
    void setGoodTime(const QTime &goodTime);
    void setAId(const int &aId);

private slots:
    void on_lineEdit_textChanged(const QString &arg1);
    void on_pushButton_clicked();

    void on_goodTimeEdit_userTimeChanged(const QTime &time);

    void on_comboBox_currentTextChanged(const QString &arg1);

    void on_bindButton_clicked();

    void on_bindIdLineEdit_textChanged(const QString &arg1);

private:
    Ui::AnswerEditDialog *ui;
    int qId;

    //ret
    QString retType;
    QString retContent;
    QTime retGoodTime;
    int retAId;

    QMap<QString,QString> type{{"自动检查","auto"},{"手动检查","manual"},{"手动检查(图片)","manual(image)"}};
    QMap<QString,QString> typeName{{"auto","自动检查"},{"manual","手动检查"},{"manual(image)","手动检查(图片)"}};
    BindAnswerDialog *bindAnswerDialog;
};

#endif // ANSWEREDITDIALOG_H
