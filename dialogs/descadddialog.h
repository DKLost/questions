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
    void setThemeBeing(const QString &themeBeing);

    QString getRetType() const;

private slots:
    void on_xLineEdit_textChanged(const QString &text);

    void on_comboBox_2_currentTextChanged(const QString &formatName);

    void on_fLineEdit_textChanged(const QString &text);

    void on_buttonBox_accepted();

    void on_mLineEdit_textChanged(const QString &text);

    void on_rLineEdit_textChanged(const QString &text);

private:
    Ui::DescAddDialog *ui;
    QString retContent;
    QString retType;
    void updatePlaceholderTexts();
    void adjustSizeByPlaceholder();
    void applyFormatState();
};

#endif // DESCADDDIALOG_H