#include "questionrenamedialog.h"
#include "ui_questionrenamedialog.h"

QuestionRenameDialog::QuestionRenameDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::QuestionRenameDialog)
{
    ui->setupUi(this);
}

QuestionRenameDialog::~QuestionRenameDialog()
{
    delete ui;
}


void QuestionRenameDialog::setText(const QString &newText)
{
    ui->lineEdit->setText(newText);
}
QString QuestionRenameDialog::getText() const
{
    return text;
}
void QuestionRenameDialog::on_lineEdit_textChanged(const QString &arg1)
{
    text = arg1;
}

