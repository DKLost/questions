#include "htmltableadddialog.h"
#include "ui_htmltableadddialog.h"

HtmlTableAddDialog::HtmlTableAddDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::HtmlTableAddDialog)
{
    ui->setupUi(this);
}

HtmlTableAddDialog::~HtmlTableAddDialog()
{
    delete ui;
}

int HtmlTableAddDialog::getRetRow() const
{
    return resultRow;
}

void HtmlTableAddDialog::setRetRow(int newRetRow)
{
    resultRow = newRetRow;
}

int HtmlTableAddDialog::getRetColumn() const
{
    return resultColumn;
}

void HtmlTableAddDialog::setRetColumn(int newRetColumn)
{
    resultColumn = newRetColumn;
}

void HtmlTableAddDialog::on_buttonBox_accepted()
{
    resultRow = ui->rowSpinBox->value();
    resultColumn = ui->ColumnSpinBox->value();
}
