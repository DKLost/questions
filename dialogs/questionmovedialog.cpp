#include "questionmovedialog.h"
#include "ui_questionmovedialog.h"

#include "core/knowledgedb.h"
#include "widgets/knowledgetreehelper.h"

#include <QStandardItemModel>

QuestionMoveDialog::QuestionMoveDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::QuestionMoveDialog)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("选择目标概念"));

    model = new QStandardItemModel(this);
    ui->phenTreeView->setModel(model);
    installReadOnlyKnowledgeTree(ui->phenTreeView);
    ui->topLevelCheckBox->setVisible(false); // 默认不出现，由 setup 按操作类型决定
    ui->topLevelCheckBox->setToolTip(QStringLiteral("勾选后挂到顶层（world 下），仅书写谓词为 has 的行可用"));
}

QuestionMoveDialog::~QuestionMoveDialog()
{
    delete knowledgeRoot;
    delete ui;
}

void QuestionMoveDialog::setup(const Args &args)
{
    expandedPhenIds = args.expandedPhenIds;
    if(!args.title.isEmpty())
        setWindowTitle(args.title);
    ui->subjLineEdit_2->setText(args.subj);
    ui->predLineEdit_2->setText(args.pred);
    ui->objLineEdit_2->setText(args.obj);
    ui->topLevelCheckBox->setVisible(args.topLevelVisible);
    ui->topLevelCheckBox->setEnabled(args.topLevelAllowed);
    ui->topLevelCheckBox->setChecked(false);

    fillTree(args.subj, args.pred, args.obj);
    if(!args.currentDerivedAbs.isEmpty())
        selectKnowledgeRowByDerivedAbs(ui->phenTreeView, args.currentDerivedAbs); // 预选即当前挂载点，直接确定＝不动
}

// 三框全空＝全树，否则走与主界面同一套检索；两种情形都恢复展开态（载入时＝主界面快照，对话框内过滤时＝对话框当前展开）
void QuestionMoveDialog::fillTree(const QString &subj, const QString &pred, const QString &obj)
{
    delete knowledgeRoot;
    const bool searching = !(subj.trimmed().isEmpty() && pred.trimmed().isEmpty() && obj.trimmed().isEmpty());
    knowledgeRoot = searching ? fillFindObjsModel(model, subj, pred, obj)
                              : fillKnowledgeModel(model);
    setupKnowledgeTreeView(ui->phenTreeView); // 模型重建后 section 重置，需重新隐藏 @ 列
    ui->phenTreeView->collapseAll();
    restoreExpandedPhenIds(ui->phenTreeView, expandedPhenIds);
}

QString QuestionMoveDialog::selectedDerivedAbs() const
{
    if(ui->topLevelCheckBox->isVisible() && ui->topLevelCheckBox->isChecked())
        return KnowledgeDB::instance()->scopePrefix(); // 顶层＝world
    return questionDerivedAbsForIndex(ui->phenTreeView, ui->phenTreeView->currentIndex()); // 折叠取合并行 is 子行，展开取父行自身
}

void QuestionMoveDialog::on_findObjsPushButton_2_clicked()
{
    expandedPhenIds = collectExpandedPhenIds(ui->phenTreeView); // 对话框内过滤只换筛选条件，不该丢掉已展开的行
    fillTree(ui->subjLineEdit_2->text(), ui->predLineEdit_2->text(), ui->objLineEdit_2->text());
}

void QuestionMoveDialog::on_phenTreeView_doubleClicked(const QModelIndex &index)
{
    if(questionDerivedAbsForIndex(ui->phenTreeView, index).isEmpty()) // 双击无效概念行不响应
        return;
    accept();
}

void QuestionMoveDialog::accept()
{
    if(selectedDerivedAbs().isEmpty()) // 未选有效概念行时忽略确定
        return;
    QDialog::accept();
}
