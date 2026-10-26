#include "questionrenamedialog.h"
#include "ui_questionrenamedialog.h"

QuestionRenameDialog::QuestionRenameDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::QuestionRenameDialog)
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
    return editedText;
}

void QuestionRenameDialog::on_lineEdit_textChanged(const QString &newText)
{
    editedText = newText; // 跟随输入实时更新，确定时无需再取值
}