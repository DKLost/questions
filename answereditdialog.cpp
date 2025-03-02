#include "answereditdialog.h"
#include "ui_answereditdialog.h"

AnswerEditDialog::AnswerEditDialog(BindAnswerDialog *mainBindAnswerDialog,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AnswerEditDialog)
{
    ui->setupUi(this);
    bindAnswerDialog = mainBindAnswerDialog;
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



int AnswerEditDialog::getQId() const
{
    return qId;
}


void AnswerEditDialog::setQId(int newQId)
{
    qId = newQId;
}



void AnswerEditDialog::lineEdit_selectAll()
{
    ui->lineEdit->setFocus();
    ui->lineEdit->selectAll();
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

void AnswerEditDialog::on_bindButton_clicked()
{
    bindAnswerDialog->initDialog(qId,retAId);
    bindAnswerDialog->exec();
    int newAId = bindAnswerDialog->getRetAId();
    if(newAId != -1 && newAId != retAId)
        ui->bindIdLineEdit->setText(QString::number(newAId));
}

//setter
void AnswerEditDialog::setType(const QString &type)
{
    ui->comboBox->setCurrentText(typeName[type]);
}
void AnswerEditDialog::setContent(const QString &content)
{
    ui->lineEdit->setText(content);
}
void AnswerEditDialog::setGoodTime(const QTime &goodTime)
{
    ui->goodTimeEdit->setTime(goodTime);
}
void AnswerEditDialog::setAId(const int &aId)
{
    ui->bindIdLineEdit->setText(QString::number(aId));
}

//getter
QTime AnswerEditDialog::getRetGoodTime() const
{
    return retGoodTime;
}
QString AnswerEditDialog::getRetContent() const
{
    return retContent;
}
QString AnswerEditDialog::getRetType() const
{
    return retType;
}
int AnswerEditDialog::getRetAId() const
{
    return retAId;
}


void AnswerEditDialog::on_comboBox_currentTextChanged(const QString &arg1)
{
    retType = type[ui->comboBox->currentText()];
}
void AnswerEditDialog::on_goodTimeEdit_userTimeChanged(const QTime &time)
{
    retGoodTime = time;
}
void AnswerEditDialog::on_lineEdit_textChanged(const QString &arg1)
{
    int pixelWide = ui->lineEdit->fontMetrics().horizontalAdvance(arg1) + 35;
    if(pixelWide < 175)
        pixelWide = 175;
    setFixedWidth(pixelWide+30);
    //ui->lineEdit->setMaximumWidth(pixelWide);
    retContent = ui->lineEdit->text();
}
void AnswerEditDialog::on_bindIdLineEdit_textChanged(const QString &arg1)
{
    retAId = arg1.toInt();
}

