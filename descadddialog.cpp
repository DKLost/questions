#include "descadddialog.h"
#include "ui_descadddialog.h"

DescAddDialog::DescAddDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DescAddDialog)
{
    ui->setupUi(this);
    int w = ui->fLineEdit->fontMetrics().horizontalAdvance("意指") + 20;
    ui->fLineEdit->setFixedWidth(w);
    w = ui->xLineEdit->fontMetrics().horizontalAdvance(ui->xLineEdit->placeholderText()) + 20;
    ui->xLineEdit->setFixedWidth(w);
    w = ui->rLineEdit->fontMetrics().horizontalAdvance(ui->rLineEdit->placeholderText()) + 20;
    ui->rLineEdit->setFixedWidth(w);
    w = ui->mLineEdit->fontMetrics().horizontalAdvance("self") + 20;
    ui->mLineEdit->setFixedWidth(w);
}
DescAddDialog::~DescAddDialog()
{
    delete ui;
}

void DescAddDialog::set_place_holder()
{
    auto f = ui->fLineEdit->text();
    auto x = ui->xLineEdit->text();
    auto r = ui->rLineEdit->text();
    auto m = ui->mLineEdit->text();
    ui->mLineEdit->setPlaceholderText("self");
    if(x.isEmpty() && r.isEmpty() && m.isEmpty())
    {
        ui->mLineEdit->setPlaceholderText("{}'");
    }
    else if(!r.isEmpty() && m.isEmpty())
    {
        ui->mLineEdit->setPlaceholderText("{}'");
    }
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

    ui->fLineEdit->setPlaceholderText("{}");

    if(ui->comboBox_2->currentText() == "标题")
    {
        ui->rLineEdit->setDisabled(true);
        ui->mLineEdit->setDisabled(true);
    }else if(ui->comboBox_2->currentText() == "有序列表")
    {
        ui->fLineEdit->setPlaceholderText("意指");
        //ui->fLineEdit->setDisabled(true);
    }

    ui->xLineEdit->setFocus();

    ui->horizontalSpacer->changeSize(0,0);
    adjustSize();
}

void DescAddDialog::on_fLineEdit_textChanged(const QString &arg1)
{
    int defaultPixelWide = ui->fLineEdit->fontMetrics().horizontalAdvance("{}") + 20;
    int pixelWide = ui->fLineEdit->fontMetrics().horizontalAdvance(arg1) + 20;
    if(pixelWide < defaultPixelWide)
        pixelWide = defaultPixelWide;
    ui->fLineEdit->setFixedWidth(pixelWide);
    set_place_holder();
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
    set_place_holder();
    adjustSize();
}
void DescAddDialog::on_rLineEdit_textChanged(const QString &arg1)
{
    int defaultPixelWide = ui->rLineEdit->fontMetrics().horizontalAdvance("意指") + 20;
    int pixelWide = ui->rLineEdit->fontMetrics().horizontalAdvance(arg1) + 20;
    if(pixelWide < defaultPixelWide)
        pixelWide = defaultPixelWide;
    ui->rLineEdit->setFixedWidth(pixelWide);
    set_place_holder();
    adjustSize();
}
void DescAddDialog::on_mLineEdit_textChanged(const QString &arg1)
{
    int defaultPixelWide = ui->mLineEdit->fontMetrics().horizontalAdvance("self") + 20;
    int pixelWide = ui->mLineEdit->fontMetrics().horizontalAdvance(arg1) + 20;
    if(pixelWide < defaultPixelWide)
        pixelWide = defaultPixelWide;
    ui->mLineEdit->setFixedWidth(pixelWide);
    set_place_holder();
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

void DescAddDialog::on_comboBox_2_currentTextChanged(const QString &arg1)
{
    QString fText = ui->fLineEdit->text();
    QString xText = ui->xLineEdit->text();
    QString rText = ui->rLineEdit->text();
    QString mText = ui->mLineEdit->text();
    reset();
    if(arg1 == "无序列表")
    {
        ui->fLineEdit->setText(fText);
        ui->xLineEdit->setText(xText);
        ui->rLineEdit->setText(rText);
        ui->mLineEdit->setText(mText);
    }else if(arg1 == "标题")
    {
        ui->fLineEdit->setText(fText);
        ui->xLineEdit->setText(xText);
        ui->rLineEdit->setDisabled(true);
        ui->mLineEdit->setDisabled(true);
    }else if(arg1 == "有序列表")
    {
        ui->fLineEdit->setPlaceholderText("意指");
        //ui->fLineEdit->setDisabled(true);
        ui->xLineEdit->setText(xText);
        ui->rLineEdit->setText(rText);
        ui->mLineEdit->setText(mText);

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
    QString f,x,r,m,s0,s1,s2;
    f = ui->fLineEdit->text();
    x = ui->xLineEdit->text();
    r = ui->rLineEdit->text();
    m = ui->mLineEdit->text();
    s0 = ": ";
    s1 = " ⇒ ";
    s2 = ": ";


    if(!f.isEmpty() && !x.isEmpty() && r.isEmpty() && m.isEmpty())
    {}else if(f.isEmpty() && x.isEmpty() && r.isEmpty() && m.isEmpty())
    {
        s0 = "";
    }else if(!f.isEmpty() && x.isEmpty() && r.isEmpty() && m.isEmpty())
    {
        s0 = ":";
    }else
    {
        if(x.isEmpty())
        {
            s0 = ":";
        }

        if(f.isEmpty())
        {
            s0 = "";
            if(x.isEmpty() && !r.isEmpty())
            {
                s1 = "";
            }
        }

        if(r.isEmpty())
        {
            s2 = "";
            if(m.isEmpty())
            {
                s1 = "";
            }
        }
    }



    retContent = f+s0+x+s1+r+s2+m;
    retType = ui->comboBox_2->currentText();
}










