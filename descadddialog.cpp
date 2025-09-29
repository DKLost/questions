#include "descadddialog.h"
#include "ui_descadddialog.h"

DescAddDialog::DescAddDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DescAddDialog)
{
    ui->setupUi(this);
}

DescAddDialog::~DescAddDialog()
{
    delete ui;
}
