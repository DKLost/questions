#include "descadddialog.h"
#include "ui_descadddialog.h"

DescAddDialog::DescAddDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DescAddDialog)
{
    ui->setupUi(this);
    int w = ui->xLineEdit->fontMetrics().horizontalAdvance(ui->xLineEdit->placeholderText()) + 20;
    ui->xLineEdit->setFixedWidth(w);
    w = ui->fLineEdit->fontMetrics().horizontalAdvance(ui->fLineEdit->placeholderText()) + 20;
    ui->fLineEdit->setFixedWidth(w);
    w = ui->rLineEdit->fontMetrics().horizontalAdvance(ui->rLineEdit->placeholderText()) + 20;
    ui->rLineEdit->setFixedWidth(w);
    w = ui->mLineEdit->fontMetrics().horizontalAdvance(ui->mLineEdit->placeholderText()) + 20;
    ui->mLineEdit->setFixedWidth(w);
}
DescAddDialog::~DescAddDialog()
{
    delete ui;
}

void DescAddDialog::reset()
{
    ui->fLineEdit->setEnabled(true);
    ui->xLineEdit->setEnabled(true);
    ui->rLineEdit->setEnabled(true);
    ui->mLineEdit->setEnabled(true);

    ui->fLineEdit->setText("");
    ui->xLineEdit->setText("");
    ui->rLineEdit->setText("");
    ui->mLineEdit->setText("");

    ui->fLineEdit->setPlaceholderText("的");

    if(ui->comboBox_2->currentText() == "标题")
    {
        ui->rLineEdit->setDisabled(true);
        ui->mLineEdit->setDisabled(true);
    }else if(ui->comboBox_2->currentText() == "有序列表")
    {
        ui->fLineEdit->setPlaceholderText("其中作为第n存在的");
        ui->fLineEdit->setDisabled(true);
    }

    ui->xLineEdit->setFocus();

    ui->horizontalSpacer->changeSize(0,0);
    adjustSize();
}

void DescAddDialog::on_fLineEdit_textChanged(const QString &arg1)
{
    int defaultPixelWide = ui->fLineEdit->fontMetrics().horizontalAdvance("的") + 20;
    int pixelWide = ui->fLineEdit->fontMetrics().horizontalAdvance(arg1) + 20;
    if(pixelWide < defaultPixelWide)
        pixelWide = defaultPixelWide;
    ui->fLineEdit->setFixedWidth(pixelWide);
    adjustSize();
}
void DescAddDialog::on_xLineEdit_textChanged(const QString &arg1)
{
    int defaultPixelWide = ui->xLineEdit->fontMetrics().horizontalAdvance("self") + 20;
    int pixelWide = ui->xLineEdit->fontMetrics().horizontalAdvance(arg1) + 20;
    if(pixelWide < defaultPixelWide)
        pixelWide = defaultPixelWide;
    ui->xLineEdit->setFixedWidth(pixelWide);
    //setFixedWidth(pixelWide+50);

    if(!arg1.isEmpty())
    {
        ui->mLineEdit->setPlaceholderText(arg1);
    }else
    {
        ui->mLineEdit->setPlaceholderText("{}");
    }
    adjustSize();
}
void DescAddDialog::on_rLineEdit_textChanged(const QString &arg1)
{
    int defaultPixelWide = ui->rLineEdit->fontMetrics().horizontalAdvance("是") + 20;
    int pixelWide = ui->rLineEdit->fontMetrics().horizontalAdvance(arg1) + 20;
    if(pixelWide < defaultPixelWide)
        pixelWide = defaultPixelWide;
    ui->rLineEdit->setFixedWidth(pixelWide);
    adjustSize();
}

QString DescAddDialog::getRetType() const
{
    return retType;
}

void DescAddDialog::setRetType(const QString &newRetType)
{
    retType = newRetType;
}
void DescAddDialog::on_mLineEdit_textChanged(const QString &arg1)
{
    int defaultPixelWide = ui->mLineEdit->fontMetrics().horizontalAdvance("{}") + 20;
    int pixelWide = ui->mLineEdit->fontMetrics().horizontalAdvance(arg1) + 20;
    if(pixelWide < defaultPixelWide)
        pixelWide = defaultPixelWide;
    ui->mLineEdit->setFixedWidth(pixelWide);
    adjustSize();
}
void DescAddDialog::on_comboBox_2_currentTextChanged(const QString &arg1)
{
    reset();
    if(arg1 == "标题")
    {
        ui->rLineEdit->setDisabled(true);
        ui->mLineEdit->setDisabled(true);

    }else if(arg1 == "有序列表")
    {
        ui->fLineEdit->setPlaceholderText("其中作为第n存在的");
        ui->fLineEdit->setDisabled(true);
    }
}


void DescAddDialog::on_DescAddDialog_accepted()
{

}

QString DescAddDialog::getRetContent() const
{
    return retContent;
}

void DescAddDialog::setRetContent(const QString &newRetContent)
{
    retContent = newRetContent;
}

void DescAddDialog::setThemeBeing(const QString &themeBeing)
{
    if(themeBeing.isEmpty())
        ui->themeBeingLabel->setText("Universe");
    else
        ui->themeBeingLabel->setText(themeBeing);
}


void DescAddDialog::on_buttonBox_accepted()
{
    QString f,x,r,m,s1,s2;
    f = ui->fLineEdit->text();
    x = ui->xLineEdit->text();
    r = ui->rLineEdit->text();
    m = ui->mLineEdit->text();
    s1 = " ⇒ ";
    s2 = " : ";

    if(f.isEmpty() && x.isEmpty() && !r.isEmpty())
    {
        s1 = "";
    }

    if(r.isEmpty())
    {
        s2 = "";
        if(m.isEmpty())
        {
            s1 = "";
        }
    }

    retContent = f+x+s1+r+s2+m;
    retType = ui->comboBox_2->currentText();
}










