#ifndef ANSWEREDITDIALOG_H
#define ANSWEREDITDIALOG_H

#include <QDialog>
#include <QMap>
#include <QFileDialog>
#include "bindanswerdialog.h"
#include <QProcess>

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
    int getRetPool() const;
    int getRetInjectBId() const; //获取注入绑定8/11

    //setter
    void setQId(int newQId);
    void setRetType(const QString &newRetType);
    void setRetContent(const QString &newRetContent);
    void setRetGoodTime(const QTime &newRetGoodTime);
    void setType(const QString &Type);
    void setContent(const QString &content);
    void setGoodTime(const QTime &goodTime);
    void setAId(const int &aId);
    void setPool(const int &pool);
    void setInjectBId(int injectBId); //设置注入绑定8/11

private slots:
    void on_lineEdit_textChanged(const QString &arg1);
    void on_pushButton_clicked();
    void on_goodTimeEdit_userTimeChanged(const QTime &time);
    void on_comboBox_currentTextChanged(const QString &arg1);
    void on_bindButton_clicked();
    void on_bindIdLineEdit_textChanged(const QString &arg1);
    void on_poolComboBox_currentTextChanged(const QString &arg1);

    void on_injectBindButton_clicked();

    void on_injectLineEdit_textChanged(const QString &arg1);
    void onTypstWatcher_standard_output();

    void on_buttonBox_accepted();

private:
    Ui::AnswerEditDialog *ui;
    int qId;
    QProcess typstWatchProcess{this};

    //ret
    QString retType;
    QString retContent;
    QTime retGoodTime;
    int retAId;
    int retPool;
    int retInjectBId;

    QMap<QString,QString> type{{"自动检查","auto"},
                                {"手动检查","manual"},
                                {"手动检查(图片)","manual(image)"},
                                {"自动检查(typst)","auto(typst)"}};
    QMap<QString,QString> typeName{{"auto","自动检查"},
                                    {"manual","手动检查"},
                                    {"manual(image)","手动检查(图片)"},
                                    {"auto(typst)","自动检查(typst)"}};
    BindAnswerDialog *bindAnswerDialog;
};

#endif // ANSWEREDITDIALOG_H
