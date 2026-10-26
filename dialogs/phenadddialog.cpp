#include "phenadddialog.h"
#include "ui_phenadddialog.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QMessageBox>

namespace
{
constexpr int kInitialLineEditWidth = 50;
constexpr int kLineEditTextPadding = 30;
const QString kAtSymbol = QStringLiteral("@");
const QString kDefaultPredicate = QStringLiteral("has");
const QString kNegatedPredicate = QStringLiteral("~has");
const QString kDefaultRelation = QStringLiteral("is");
}

PhenAddDialog::PhenAddDialog(QWidget *parent) : QDialog(parent),
                                                ui(new Ui::PhenAddDialog)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("新增现象"));
    connect(ui->atFlag1, &QComboBox::currentTextChanged, this,
            [this](const QString &) { syncFPredicateDefault(); }); // (∃) 形式切换时同步 f 的缺省谓词（: has / @ ~has）
    fitWidth(ui->fLineEdit_3); // 初态按占位符宽度收缩
    fitWidth(ui->xLineEdit_3);
    fitWidth(ui->rLineEdit_3);
    fitWidth(ui->mLineEdit_3);
    adjustSize();
}

PhenAddDialog::~PhenAddDialog()
{
    delete ui;
}

// 与笔记语法一致：- X 缺省 has，- @X 缺省 ~has
void PhenAddDialog::syncFPredicateDefault()
{
    const bool isAtPrefix = (ui->atFlag1->currentText() == kAtSymbol);
    ui->fLineEdit_3->setPlaceholderText(isAtPrefix ? kNegatedPredicate : kDefaultPredicate);
    fitWidth(ui->fLineEdit_3);
    adjustSize();
}

void PhenAddDialog::fitWidth(QLineEdit *edit)
{
    const QString basis = edit->text().isEmpty() ? edit->placeholderText() : edit->text();
    int width = edit->fontMetrics().horizontalAdvance(basis) + kLineEditTextPadding;
    width = qMax(width, kInitialLineEditWidth);
    edit->setFixedWidth(width);
}

// 顶层概念勾选：world 下强制 f=has 且 (∃) 形式强制为 :
void PhenAddDialog::on_topLevelCheckBox_toggled(bool checked)
{
    ui->fLineEdit_3->setEnabled(!checked);
    if(checked)
        ui->fLineEdit_3->clear(); // 还原为显示缺省谓词 has

    ui->atFlag1->setEnabled(!checked);
    if(checked)
        ui->atFlag1->setCurrentIndex(0);

    syncFPredicateDefault();
}

bool PhenAddDialog::isTopLevel() const { return ui->topLevelCheckBox->isChecked(); }

void PhenAddDialog::setTopLevel(bool checked, bool disable)
{
    ui->topLevelCheckBox->setChecked(checked);
    ui->topLevelCheckBox->setEnabled(!disable); // 未选中节点进入时禁用取消
}

// 编辑既有现象行的预填：先固定顶层勾选（置灰）再填四段，避免勾选处理清空 f；m 为空时第二段整段缺省
void PhenAddDialog::setup(const QString &f, const QString &x, bool atFlag1,
                          const QString &r, const QString &m, bool atFlag2, bool topLevel)
{
    setWindowTitle(QStringLiteral("编辑现象"));
    setTopLevel(topLevel, true);
    ui->fLineEdit_3->setText(f);
    ui->xLineEdit_3->setText(x);
    ui->rLineEdit_3->setText(r);
    ui->mLineEdit_3->setText(m);
    ui->atFlag1->setCurrentIndex(atFlag1 ? 1 : 0);
    ui->atFlag2->setCurrentIndex(atFlag2 ? 1 : 0);

    syncFPredicateDefault();
    fitWidth(ui->fLineEdit_3);
    fitWidth(ui->xLineEdit_3);
    fitWidth(ui->rLineEdit_3);
    fitWidth(ui->mLineEdit_3);
    adjustSize();
}

QString PhenAddDialog::getF() const { return retF; }
QString PhenAddDialog::getX() const { return retX; }
QString PhenAddDialog::getR() const { return retR; }
QString PhenAddDialog::getM() const { return retM; }
bool PhenAddDialog::getAtFlag1() const { return isAtPrefixedF; }
bool PhenAddDialog::getAtFlag2() const { return isAtPrefixedR; }

QString PhenAddDialog::readFPredicate() const
{
    const QString predicate = ui->fLineEdit_3->text().trimmed();
    if(!predicate.isEmpty())
        return predicate;
    return isAtPrefixedF ? kNegatedPredicate : kDefaultPredicate;
}

void PhenAddDialog::readRelationAndObject()
{
    const QString object = ui->mLineEdit_3->text().trimmed();
    if(object.isEmpty())
    {
        retM.clear();
        retR.clear();
        return;
    }

    retM = object;
    retR = ui->rLineEdit_3->text().trimmed();
    if(retR.isEmpty())
        retR = kDefaultRelation;
}

void PhenAddDialog::accept()
{
    const QString center = ui->xLineEdit_3->text().trimmed();
    if(center.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("输入不完整"),
                             QStringLiteral("中心存在（x）不能为空"));
        return;
    }

    isAtPrefixedF = (ui->atFlag1->currentText() == kAtSymbol);
    isAtPrefixedR = (ui->atFlag2->currentText() == kAtSymbol);

    retX = center;
    retF = readFPredicate();
    readRelationAndObject();

    QDialog::accept();
}

void PhenAddDialog::on_fLineEdit_3_textChanged(const QString &) { fitWidth(ui->fLineEdit_3); adjustSize(); }
void PhenAddDialog::on_xLineEdit_3_textChanged(const QString &) { fitWidth(ui->xLineEdit_3); adjustSize(); }
void PhenAddDialog::on_rLineEdit_3_textChanged(const QString &) { fitWidth(ui->rLineEdit_3); adjustSize(); }
void PhenAddDialog::on_mLineEdit_3_textChanged(const QString &) { fitWidth(ui->mLineEdit_3); adjustSize(); }