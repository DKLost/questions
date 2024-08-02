#include "questionmovedialog.h"
#include "ui_questionmovedialog.h"
#include <QModelIndex>

QuestionMoveDialog::QuestionMoveDialog(QStandardItemModel *categoryItemModel,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::QuestionMoveDialog)
{
    ui->setupUi(this);
    ui->treeView->setModel(categoryItemModel);

    ui->treeView->header()->setStretchLastSection(false);
    ui->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->treeView->header()->setSectionResizeMode(0,QHeaderView::Stretch);
    ui->treeView->hideColumn(1);
}

QuestionMoveDialog::~QuestionMoveDialog()
{
    delete ui;
}

void QuestionMoveDialog::on_buttonBox_accepted()
{
    QModelIndex index = ui->treeView->currentIndex();
    int id = index.siblingAtColumn(1).data().toInt();
    retId = id;
}

int QuestionMoveDialog::getRetId() const
{
    return retId;
}

void QuestionMoveDialog::setRetId(int newRetId)
{
    retId = newRetId;
}

