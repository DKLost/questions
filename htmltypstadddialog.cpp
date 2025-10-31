#include "htmltypstadddialog.h"
#include "ui_htmltypstadddialog.h"

HtmlTypstAddDialog::HtmlTypstAddDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HtmlTypstAddDialog)
{
    ui->setupUi(this);
    typstWatchProcess.setProcessChannelMode(QProcess::MergedChannels);
    connect(&typstWatchProcess, &QProcess::readyReadStandardOutput, this, &HtmlTypstAddDialog::onTypstWatcher_standard_output);
}

HtmlTypstAddDialog::~HtmlTypstAddDialog()
{
    delete ui;
}
void HtmlTypstAddDialog::reset()
{
    ui->lineEdit->clear();
    ui->label->clear();
    retContent = "";
    retPath = "";
    if(typstWatchProcess.state() != QProcess::NotRunning)
        ToolFunctions::watch_typst_stop(typstWatchProcess);
}
void HtmlTypstAddDialog::on_lineEdit_textChanged(const QString &arg1)
{
    int pixelWide = ui->lineEdit->fontMetrics().horizontalAdvance(arg1) + 35;
    if(pixelWide < 200)
        pixelWide = 200;
    setFixedWidth(pixelWide+30);
    //ui->lineEdit->setMaximumWidth(pixelWide);
    retContent = ui->lineEdit->text();
    QString typst = ToolFunctions::typstMathPrefix;
    typst = typst + "$ " + ui->lineEdit->text() + " $";
    if(typstWatchProcess.state() == QProcess::NotRunning)
        ToolFunctions::watch_typst_start(typstWatchProcess,"temp.typ","temp.png");
    ToolFunctions::write_typst(typst,"temp.typ");
}


void HtmlTypstAddDialog::on_buttonBox_accepted()
{
    QString hash = QCryptographicHash::hash(ui->lineEdit->text().toUtf8(),
                                            QCryptographicHash::Sha256).toHex().left(32);
    //QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(typstWatchProcess.state() != QProcess::NotRunning)
        ToolFunctions::watch_typst_stop(typstWatchProcess);
    retPath = QString("./data/%1/%2.png").arg(qId).arg(hash);
    QString mathPath = QString("./data/%1/%2.typmath").arg(qId).arg(hash);
    ToolFunctions::write_file(mathPath,ui->lineEdit->text());
    ToolFunctions::compile_typst("temp.typ",retPath);
}

void HtmlTypstAddDialog::onTypstWatcher_standard_output()
{
    ui->label->setPixmap(QPixmap{"temp.png"});
}

QString HtmlTypstAddDialog::getRetPath() const
{
    return retPath;
}

void HtmlTypstAddDialog::setRetPath(const QString &newRetPath)
{
    retPath = newRetPath;
}

void HtmlTypstAddDialog::setText(const QString &text)
{
    ui->lineEdit->setText(text);
}

int HtmlTypstAddDialog::getQId() const
{
    return qId;
}

void HtmlTypstAddDialog::setQId(int newQId)
{
    qId = newQId;
}
