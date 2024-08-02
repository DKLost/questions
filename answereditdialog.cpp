#include "answereditdialog.h"
#include "ui_answereditdialog.h"

AnswerEditDialog::AnswerEditDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AnswerEditDialog)
{
    ui->setupUi(this);
}

AnswerEditDialog::~AnswerEditDialog()
{
    delete ui;
}

QString AnswerEditDialog::getRetString() const
{
    return retString;
}

void AnswerEditDialog::setRetString(const QString &newRetString)
{
    retString = newRetString;
}

void AnswerEditDialog::clearTextEdit()
{
    ui->lineEdit->clear();
}

void AnswerEditDialog::setTextEdit(QString text)
{
    ui->lineEdit->setText(text);
}

void AnswerEditDialog::lineEdit_selectAll()
{
    ui->lineEdit->setFocus();
    ui->lineEdit->selectAll();
}

void AnswerEditDialog::on_buttonBox_accepted()
{
    retString = ui->lineEdit->text();
}

