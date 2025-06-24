#ifndef SETTIMEDIALOG_H
#define SETTIMEDIALOG_H

#include <QDialog>
#include <QTime>

namespace Ui {
class SetTimeDialog;
}

class SetTimeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SetTimeDialog(QWidget *parent = nullptr);
    ~SetTimeDialog();

    QTime getTime() const;
    void setTime(const QTime &newTime);
    void initTime(const QTime &newTime);

private slots:
    void on_buttonBox_accepted();

private:
    Ui::SetTimeDialog *ui;
    QTime time;
};

#endif // SETTIMEDIALOG_H
