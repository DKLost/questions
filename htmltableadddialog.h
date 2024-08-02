#ifndef HTMLTABLEADDDIALOG_H
#define HTMLTABLEADDDIALOG_H

#include <QDialog>

namespace Ui {
class HtmlTableAddDialog;
}

class HtmlTableAddDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HtmlTableAddDialog(QWidget *parent = nullptr);
    ~HtmlTableAddDialog();

    int getRetRow() const;
    void setRetRow(int newRetRow);

    int getRetColumn() const;
    void setRetColumn(int newRetColumn);

private slots:
    void on_buttonBox_accepted();

private:
    Ui::HtmlTableAddDialog *ui;
    int retRow;
    int retColumn;
};

#endif // HTMLTABLEADDDIALOG_H
