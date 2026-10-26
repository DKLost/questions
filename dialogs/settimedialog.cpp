#include "settimedialog.h"
#include "ui_settimedialog.h"

namespace {
constexpr int kInvalidTimeMsecs = -1; // 用户未点确定时用于保持无效时间
}

SetTimeDialog::SetTimeDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::SetTimeDialog)
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
    return selectedTime;
}

void SetTimeDialog::setTime(const QTime &newTime)
{
    selectedTime = newTime;
    ui->timeEdit->setTime(selectedTime);
}

void SetTimeDialog::initTime(const QTime &newTime)
{
    selectedTime = QTime::fromMSecsSinceStartOfDay(kInvalidTimeMsecs); // 先置无效：用户未点确定则 getTime 返回无效值
    ui->timeEdit->setTime(newTime);
}

void SetTimeDialog::on_buttonBox_accepted()
{
    selectedTime = ui->timeEdit->time();
}