#include "answereditdialog.h"
#include "ui_answereditdialog.h"

AnswerEditDialog::AnswerEditDialog(BindAnswerDialog *mainBindAnswerDialog,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AnswerEditDialog)
{
    ui->setupUi(this);
    bindAnswerDialog = mainBindAnswerDialog;
    retPool = 0;
    ui->displayLabel->hide();
    typstWatchProcess.setProcessChannelMode(QProcess::MergedChannels);
    connect(&typstWatchProcess, &QProcess::readyReadStandardOutput, this, &AnswerEditDialog::onTypstWatcher_standard_output);
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

void AnswerEditDialog::setPool(const int &pool)
{
    ui->poolComboBox->setCurrentText(QString::number(pool));
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
int AnswerEditDialog::getRetPool() const
{
    return retPool;
}


void AnswerEditDialog::on_comboBox_currentTextChanged(const QString &arg1)
{
    retType = type[ui->comboBox->currentText()];
    if(retType == "auto(typst)")
        ui->displayLabel->show();
    else
        ui->displayLabel->hide();
}
void AnswerEditDialog::on_goodTimeEdit_userTimeChanged(const QTime &time)
{
    retGoodTime = time;
}
void AnswerEditDialog::on_lineEdit_textChanged(const QString &arg1)
{
    int pixelWide = ui->lineEdit->fontMetrics().horizontalAdvance(arg1) + 35;
    if(pixelWide < 200)
        pixelWide = 200;
    setFixedWidth(pixelWide+30);
    //ui->lineEdit->setMaximumWidth(pixelWide);
    retContent = ui->lineEdit->text();

    if(type[ui->comboBox->currentText()] == "auto(typst)")
    {
        QString typst = "#set page(height: auto,width: auto,margin: 0cm);";
        typst = typst + "$ " + ui->lineEdit->text() + " $";
        if(typstWatchProcess.state() == QProcess::NotRunning)
            ToolFunctions::watch_typst_start(typstWatchProcess,"temp.typ","temp.png");
        ToolFunctions::write_typst(typst,"temp.typ");
    }else
    {
        if(typstWatchProcess.state() != QProcess::NotRunning)
            ToolFunctions::watch_typst_stop(typstWatchProcess);
    }
}
void AnswerEditDialog::onTypstWatcher_standard_output()
{
    ui->displayLabel->setPixmap(QPixmap{"temp.png"});
}

void AnswerEditDialog::on_bindIdLineEdit_textChanged(const QString &arg1)
{
    retAId = arg1.toInt();
}
void AnswerEditDialog::on_poolComboBox_currentTextChanged(const QString &arg1)
{
    retPool = arg1.toInt();
}


//添加简化的注入绑定功能8/11
void AnswerEditDialog::on_injectBindButton_clicked()
{
    bindAnswerDialog->initDialog(qId,retAId);
    bindAnswerDialog->exec();
    int newAId = bindAnswerDialog->getRetAId();
    if(newAId != -1 && newAId != retAId)
        ui->injectLineEdit->setText(QString::number(newAId));
}
int AnswerEditDialog::getRetInjectBId() const
{
    return retInjectBId;
}
void AnswerEditDialog::setInjectBId(int injectBId)
{
    ui->injectLineEdit->setText(QString::number(injectBId));
}
void AnswerEditDialog::on_injectLineEdit_textChanged(const QString &arg1)
{
    retInjectBId = arg1.toInt();
}

void AnswerEditDialog::on_buttonBox_accepted()
{
    if(typstWatchProcess.state() != QProcess::NotRunning)
        ToolFunctions::watch_typst_stop(typstWatchProcess);
    QString filePath = QString("./data/%1/%2.png").arg(qId).arg(retAId);
    ToolFunctions::compile_typst("temp.typ",filePath);
}

