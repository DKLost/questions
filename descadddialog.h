#ifndef DESCADDDIALOG_H
#define DESCADDDIALOG_H

#include <QDialog>

namespace Ui {
class DescAddDialog;
}

class DescAddDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DescAddDialog(QWidget *parent = nullptr);
    ~DescAddDialog();

    void reset();
    QString getRetContent() const;
    void setRetContent(const QString &newRetContent);
    void setThemeBeing(const QString &themeBeing);

    QString getRetType() const;
    void setRetType(const QString &newRetType);

private slots:
    void on_xLineEdit_textChanged(const QString &arg1);

    void on_comboBox_2_currentTextChanged(const QString &arg1);

    void on_fLineEdit_textChanged(const QString &arg1);

    void on_buttonBox_accepted();

    void on_DescAddDialog_accepted();

    void on_mLineEdit_textChanged(const QString &arg1);

    void on_rLineEdit_textChanged(const QString &arg1);

private:
    Ui::DescAddDialog *ui;
    QString retContent;
    QString retType;
    void set_place_holder();
    void adjust_size_by_place_holder();
};

#endif // DESCADDDIALOG_H
