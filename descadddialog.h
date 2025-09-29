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

private:
    Ui::DescAddDialog *ui;
};

#endif // DESCADDDIALOG_H
