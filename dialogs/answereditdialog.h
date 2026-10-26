#ifndef ANSWEREDITDIALOG_H
#define ANSWEREDITDIALOG_H

#include <QDialog>
#include <QJsonArray>
#include <QProcess>
#include <QString>
#include <QTime>

class BindAnswerDialog;

namespace Ui {
class AnswerEditDialog;
}

class AnswerEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AnswerEditDialog(BindAnswerDialog *mainBindAnswerDialog, QWidget *parent = nullptr);
    ~AnswerEditDialog();

    void resetEdit();
    void lineEdit_selectAll();

    QString getRetType() const;
    QString getRetContent() const;
    QTime getRetGoodTime() const;
    int getRetAId() const;
    int getRetPool() const;
    int getRetInjectBId() const;

    void setQId(int newQId);
    void setType(const QString &Type);
    void setContent(const QString &content);
    void setGoodTime(const QTime &goodTime);
    void setAId(const int &aId);
    void setPool(const int &pool);
    void setInjectBId(int injectBId);
    void setSiblingAnswers(const QJsonArray &answers); // 供链卡同题唯一校验

private slots:
    void on_lineEdit_textChanged(const QString &text);
    void on_pushButton_clicked();
    void on_goodTimeEdit_userTimeChanged(const QTime &time);
    void on_comboBox_currentTextChanged(const QString &displayName);
    void on_bindButton_clicked();
    void on_bindIdLineEdit_textChanged(const QString &text);
    void on_poolComboBox_currentTextChanged(const QString &text);

    void on_injectBindButton_clicked();

    void on_injectLineEdit_textChanged(const QString &text);
    void onTypstWatcher_standard_output();

    void on_buttonBox_accepted();

protected:
    void accept() override; // 链卡不唯一时阻止关闭

private:
    int pickBoundAnswerId();
    bool hasOtherChainCard() const;
    void applyTypstAnswerMode();
    void applyChainAnswerMode();
    void applyDefaultAnswerMode();
    void stopTypstPreview();
    void updateLineEditWidth(const QString &text);
    void updateTypstPreview();
    void refreshTypstPreviewImage();

    Ui::AnswerEditDialog *ui;
    int questionId;
    QProcess typstWatchProcess{this}; // typst 实时预览进程

    QString retType;
    QString retContent;
    QTime retGoodTime;
    int retAId;
    int retPool;
    int retInjectBId;
    QJsonArray siblingAnswers;

    BindAnswerDialog *bindAnswerDialog;
};

#endif // ANSWEREDITDIALOG_H