#include "clozemovedialog.h"
#include "ui_clozemovedialog.h"

#include "core/questionsql.h"
#include "widgets/knowledgetreehelper.h"
#include "utils/toolfunctions.h"

#include <QStandardItemModel>
#include <QSqlRelationalTableModel>
#include <QHeaderView>
#include <QMessageBox>

namespace
{
constexpr int kIdColumn = 0;
constexpr int kNameColumn = 1;
constexpr int kNoQuestionId = -1;
const QString kQuestionsTableName = QStringLiteral("questions");
const QString kChainSeparator = QStringLiteral("→");    // DerivedAbs 名段分隔符
const QString kNoMatchFilter = QStringLiteral("1 = 0"); // 未选中概念时过滤掉全部题目

// DerivedAbs 过滤条件：命中该名自身（abstractName = 该名）及其子树（该名→ 前缀，段边界精确）
QString questionFilterForDerivedAbs(const QString &derivedAbs, int excludeQId)
{
    if(derivedAbs.isEmpty())
        return kNoMatchFilter;

    const QString escaped = QString(derivedAbs).replace('\'', "''");
    QString filter = QString("(abstractName = '%1' OR substr(abstractName,1,%2) = '%1%3')")
                         .arg(escaped)
                         .arg(derivedAbs.size() + kChainSeparator.size())
                         .arg(kChainSeparator);
    if(excludeQId > 0) // 源题目自身不作为目标
        filter = ToolFunctions::andFilters(filter, QStringLiteral("questions.id != %1").arg(excludeQId));
    return filter;
}
}

ClozeMoveDialog::ClozeMoveDialog(QuestionSql *newQuestionSql, QWidget *parent)
    : QDialog(parent), ui(new Ui::ClozeMoveDialog), questionSql(newQuestionSql)
{
    ui->setupUi(this);
    setupKnowledgeTree();
    setupQuestionTreeView();
}

ClozeMoveDialog::~ClozeMoveDialog()
{
    delete knowledgeRoot;
    delete ui;
}

void ClozeMoveDialog::setupKnowledgeTree()
{
    model = new QStandardItemModel(this);
    ui->phenTreeView->setModel(model);
    installReadOnlyKnowledgeTree(ui->phenTreeView);
}

void ClozeMoveDialog::setupQuestionTreeView()
{
    questionTableModel = new QSqlRelationalTableModel(this, questionSql->getDb());
    questionTableModel->setTable(kQuestionsTableName);
    questionTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    ui->questionsTreeView->setModel(questionTableModel);
    ui->questionsTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->questionsTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->questionsTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->questionsTreeView->setRootIsDecorated(false);
    ui->questionsTreeView->setContextMenuPolicy(Qt::NoContextMenu);
}

void ClozeMoveDialog::setup(const QString &sourceDerivedAbs, const QSet<int> &expandedPhenIds, int newExcludeQId)
{
    excludeQId = newExcludeQId;

    delete knowledgeRoot;
    knowledgeRoot = fillKnowledgeModel(model);
    setupKnowledgeTreeView(ui->phenTreeView); // 模型重建后 section 重置，需重新隐藏 @ 列
    ui->phenTreeView->collapseAll();
    restoreExpandedPhenIds(ui->phenTreeView, expandedPhenIds); // 沿用主界面知识树的展开状态

    if(!sourceDerivedAbs.isEmpty())
        selectKnowledgeRowByDerivedAbs(ui->phenTreeView, sourceDerivedAbs);
    loadQuestionsForDerivedAbs(questionDerivedAbsForIndex(ui->phenTreeView, ui->phenTreeView->currentIndex()));
}

void ClozeMoveDialog::loadQuestionsForDerivedAbs(const QString &derivedAbs)
{
    questionTableModel->setFilter(questionFilterForDerivedAbs(derivedAbs, excludeQId));
    questionTableModel->select();
    ToolFunctions::fetchAllRows(questionTableModel);
    configureQuestionColumns();
}

// 只呈现 id 与名称两列，并默认选中首行
void ClozeMoveDialog::configureQuestionColumns()
{
    for(int column = 0; column < questionTableModel->columnCount(); ++column)
        ui->questionsTreeView->header()->setSectionHidden(column, column != kIdColumn && column != kNameColumn);
    ui->questionsTreeView->header()->setSectionResizeMode(kIdColumn, QHeaderView::ResizeToContents);
    ui->questionsTreeView->header()->setSectionResizeMode(kNameColumn, QHeaderView::Stretch);
    if(questionTableModel->rowCount() > 0)
        ui->questionsTreeView->setCurrentIndex(questionTableModel->index(0, kNameColumn));
}

void ClozeMoveDialog::on_phenTreeView_clicked(const QModelIndex &index)
{
    loadQuestionsForDerivedAbs(questionDerivedAbsForIndex(ui->phenTreeView, index));
}

void ClozeMoveDialog::on_questionsTreeView_doubleClicked(const QModelIndex &index)
{
    if(index.isValid())
        accept();
}

int ClozeMoveDialog::selectedQId() const
{
    const QModelIndex index = ui->questionsTreeView->currentIndex();
    if(!index.isValid())
        return kNoQuestionId;
    const int qId = questionTableModel->index(index.row(), kIdColumn).data().toInt();
    return qId > 0 ? qId : kNoQuestionId;
}

void ClozeMoveDialog::accept()
{
    if(selectedQId() <= 0) // 未选目标题目时不允许确定
    {
        QMessageBox::information(this, QStringLiteral("移动答案"),
                                 QStringLiteral("请先在右侧选择要移入的题目。"));
        return;
    }
    QDialog::accept();
}