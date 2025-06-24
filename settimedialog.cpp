#include "settimedialog.h"
#include "ui_settimedialog.h"

SetTimeDialog::SetTimeDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SetTimeDialog)
{
    ui->setupUi(this);
    ui->timeEdit->setCurrentSection(QDateTimeEdit::SecondSection);
}

SetTimeDialog::~SetTimeDialog()
{
    delete ui;
}

QTime SetTimeDialog::getTime() const
{
    return time;
}

void SetTimeDialog::setTime(const QTime &newTime)
{
    time = newTime;
    ui->timeEdit->setTime(time);
}

void SetTimeDialog::initTime(const QTime &newTime)
{
    time = QTime::fromMSecsSinceStartOfDay(-1);
    ui->timeEdit->setTime(newTime);
}

void SetTimeDialog::on_buttonBox_accepted()
{
    time = ui->timeEdit->time();
}
