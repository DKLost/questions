#include "htmltableadddialog.h"
#include "ui_htmltableadddialog.h"

HtmlTableAddDialog::HtmlTableAddDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HtmlTableAddDialog)
{
    ui->setupUi(this);
}

HtmlTableAddDialog::~HtmlTableAddDialog()
{
    delete ui;
}

int HtmlTableAddDialog::getRetRow() const
{
    return retRow;
}

void HtmlTableAddDialog::setRetRow(int newRetRow)
{
    retRow = newRetRow;
}

int HtmlTableAddDialog::getRetColumn() const
{
    return retColumn;
}

void HtmlTableAddDialog::setRetColumn(int newRetColumn)
{
    retColumn = newRetColumn;
}

void HtmlTableAddDialog::on_buttonBox_accepted()
{
    retRow = ui->rowSpinBox->value();
    retColumn = ui->ColumnSpinBox->value();
}

