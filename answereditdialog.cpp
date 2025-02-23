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

void AnswerEditDialog::resetEdit()
{
    ui->lineEdit->clear();
    retContent = "";
    ui->comboBox->setCurrentText("自动检查");
    retType = "auto";
}

QString AnswerEditDialog::getRetContent() const
{
    return retContent;
}

void AnswerEditDialog::setRetContent(const QString &newRetContent)
{
    retContent = newRetContent;
}

void AnswerEditDialog::setType(const QString &type)
{
    ui->comboBox->setCurrentText(typeName[type]);
}

void AnswerEditDialog::setContent(const QString &content)
{
    ui->lineEdit->setText(content);
}

int AnswerEditDialog::getQId() const
{
    return qId;
}

void AnswerEditDialog::setQId(int newQId)
{
    qId = newQId;
}

QString AnswerEditDialog::getRetType() const
{
    return retType;
}

void AnswerEditDialog::setRetType(const QString &newRetType)
{
    retType = newRetType;
}

void AnswerEditDialog::lineEdit_selectAll()
{
    ui->lineEdit->setFocus();
    ui->lineEdit->selectAll();
}

void AnswerEditDialog::on_buttonBox_accepted()
{
    retContent = ui->lineEdit->text();
    retType = type[ui->comboBox->currentText()];
}

void AnswerEditDialog::on_lineEdit_textChanged(const QString &arg1)
{
    int pixelWide = ui->lineEdit->fontMetrics().horizontalAdvance(arg1) + 35;
    if(pixelWide < 175)
        pixelWide = 175;
    setFixedWidth(pixelWide+30);
    //ui->lineEdit->setMaximumWidth(pixelWide);
}

void AnswerEditDialog::on_pushButton_clicked()
{
    if(ui->comboBox->currentText() == "手动检查(图片)")
    {
        QString dirPath = QString("%1/data/%2/").arg(QDir::currentPath()).arg(qId);
        QString filePath = QFileDialog::getOpenFileName(this,
                                                        "打开图片", dirPath, tr("Image Files (*.jpg *.png *.gif)"));
        if(filePath.isNull() || filePath.isEmpty() || filePath == "")
            return;
        QDir dir("./");
        ui->lineEdit->setText(dir.relativeFilePath(filePath));
    }
}

