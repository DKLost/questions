#include "bindanswerdialog.h"
#include "ui_bindanswerdialog.h"

#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QSize>
#include <QSplitter>

namespace {
const int kTabStopSpaces = 4;
const int kSplitterCategorySize = 250;
const int kSplitterQuestionSize = 600;
const int kSplitterAnswerSize = 350;
const int kNoStretch = 0;
const int kStretch = 1;
const int kRowStepUp = -1;
const int kRowStepDown = 1;
const int kInvalidAnswerId = -1;

const QSize kAnswerIconSize{1000, 1000};

const QString kQuestionTable = QStringLiteral("questions");
const QString kClozeTable = QStringLiteral("clozes");
const QString kAnswerTypeManualImage = QStringLiteral("manual(image)");
const QString kAnswerFilterFormat = QStringLiteral("id = %1");
const QString kJsonFieldId = QStringLiteral("id");
const QString kJsonFieldType = QStringLiteral("type");
const QString kJsonFieldContent = QStringLiteral("content");

void initSplitter(QSplitter *splitter)
{
    splitter->setSizes({kSplitterCategorySize, kSplitterQuestionSize, kSplitterAnswerSize});
    splitter->setStretchFactor(0, kNoStretch);
    splitter->setStretchFactor(1, kNoStretch);
    splitter->setStretchFactor(2, kStretch);
}

void applyAnswerItemAppearance(QListWidgetItem *item, const QString &answerType, const QString &content)
{
    if(answerType == kAnswerTypeManualImage)
        item->setIcon(QIcon{content});
    else
        item->setText(content);
}

void populateAnswerList(QListWidget *answerListWidget, const QJsonArray &answers)
{
    answerListWidget->clear();
    answerListWidget->setIconSize(kAnswerIconSize);
    for(const QJsonValue &answerValue : answers)
    {
        const QJsonObject answerObject = answerValue.toObject();
        const QString answerType = answerObject.value(kJsonFieldType).toString();
        const QString content = answerObject.value(kJsonFieldContent).toString();
        auto *item = new QListWidgetItem; // id:StatusTip,type:ToolTip,content:WhatsThis
        item->setStatusTip(QString::number(answerObject.value(kJsonFieldId).toInt()));
        item->setToolTip(answerType);
        item->setWhatsThis(content);
        applyAnswerItemAppearance(item, answerType, content);
        answerListWidget->addItem(item);
    }
}
} // namespace

BindAnswerDialog::BindAnswerDialog(QuestionSql *mainQuestionSql,
                                   QTreeView *mainCategoryTreeView,
                                   QTableView *mainQuestionTableView,
                                   QWidget *parent)
    : QDialog(parent), ui(new Ui::BindAnswerDialog)
{
    ui->setupUi(this);
    questionSql = mainQuestionSql;
    this->mainQuestionTableView = mainQuestionTableView;
    selectedAnswerId = kInvalidAnswerId; // 未 initDialog 前视为未选中，避免读到未初始化值
    Q_UNUSED(mainCategoryTreeView); // 分类功能已移除，入参保留以兼容既有调用
    init_questionTableView();
    init_answerTableView();
    ui->categoryTreeView->setEnabled(false); // 分类已移除，保留 ui 占位
    ui->questionTextEdit->setTabStopDistance(ui->questionTextEdit->fontMetrics().horizontalAdvance(' ') * kTabStopSpaces);
    initSplitter(ui->splitter);
}

void BindAnswerDialog::setTreeModel(QAbstractItemModel *model)
{
    ui->categoryTreeView->setModel(model);
    ui->categoryTreeView->header()->setStretchLastSection(false);
    ui->categoryTreeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->categoryTreeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->categoryTreeView->hideColumn(1);
}

void BindAnswerDialog::disableCategoryTree()
{
    ui->categoryTreeView->setModel(nullptr);
    ui->categoryTreeView->setEnabled(false);
}

BindAnswerDialog::~BindAnswerDialog()
{
    delete ui;
}

void BindAnswerDialog::init_answerTableView()
{
    answerTableModel = new QSqlTableModel(this, questionSql->getDb());
    answerTableModel->setTable(kClozeTable);
    answerTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);

    ui->answerTableView->setModel(answerTableModel);
    ui->answerTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    setAnswerTableHeaders(answerTableModel);
}
void BindAnswerDialog::on_answerListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    if(current == nullptr)
        return;
    answerTableModel->setFilter(kAnswerFilterFormat.arg(current->statusTip().toInt()));
    answerTableModel->select();
}

void BindAnswerDialog::init_questionTableView()
{
    questionTableModel = new QSqlRelationalTableModel(this, questionSql->getDb());
    questionTableModel->setTable(kQuestionTable);
    questionTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    questionTableModel->select();

    setupQuestionTableView(ui->questionTableView, questionTableModel);
    ui->questionTableView->installEventFilter(this);
}
void BindAnswerDialog::select_question_by_id(int id)
{
    if(id < 0)
        return;
    const QModelIndexList matchedIndexes = questionTableModel->match(questionTableModel->index(0, QuestionIdColumn), Qt::DisplayRole, id);
    const QModelIndex index = matchedIndexes.first();
    ui->questionTableView->setCurrentIndex(index);
    on_questionTableView_activated(index);
}
void BindAnswerDialog::on_questionTableView_clicked(const QModelIndex &index)
{
    on_questionTableView_activated(index);
}
void BindAnswerDialog::on_questionTableView_entered(const QModelIndex &index)
{
    if(QGuiApplication::mouseButtons() == Qt::NoButton)
        return;
    if(QGuiApplication::mouseButtons() == Qt::RightButton)
        return;
    on_questionTableView_activated(index);
}
void BindAnswerDialog::on_questionTableView_activated(const QModelIndex &index)
{
    if(!index.isValid())
        return;
    const int questionId = index.siblingAtColumn(QuestionIdColumn).data().toInt();
    if(questionId <= 0)
        return;
    ui->questionTextEdit->setHtml(questionSql->read_questionHTML(questionId));
    populateAnswerList(ui->answerListWidget, questionSql->read_answerJSON(questionId));
}

// category（已禁用）
void BindAnswerDialog::on_categoryTreeView_clicked(const QModelIndex &)
{
}

bool BindAnswerDialog::eventFilter(QObject *watched, QEvent *event)
{
    if(watched != ui->questionTableView)
        return BindAnswerDialog::eventFilter(watched, event); // 其余对象交回基类
    if(event->type() != QEvent::KeyPress)
        return false;
    const int key = static_cast<QKeyEvent *>(event)->key();
    if(key == Qt::Key_Up)
        moveQuestionSelection(kRowStepUp);
    else if(key == Qt::Key_Down)
        moveQuestionSelection(kRowStepDown);
    return true;
}

void BindAnswerDialog::moveQuestionSelection(int rowDelta)
{
    const QModelIndex current = ui->questionTableView->currentIndex();
    if(!current.isValid())
        return;
    const int targetRow = current.row() + rowDelta;
    if(targetRow < 0 || targetRow >= ui->questionTableView->model()->rowCount())
        return;
    ui->questionTableView->selectRow(targetRow);
    const QModelIndex next = ui->questionTableView->currentIndex();
    if(next.isValid())
        on_questionTableView_activated(next);
}

int BindAnswerDialog::getRetAId() const
{
    return selectedAnswerId;
}

void BindAnswerDialog::setRetAId(int newRetAId)
{
    selectedAnswerId = newRetAId;
}

void BindAnswerDialog::on_buttonBox_accepted()
{
    QListWidgetItem *currentItem = ui->answerListWidget->currentItem();
    if(currentItem == nullptr)
        selectedAnswerId = kInvalidAnswerId;
    else
        selectedAnswerId = currentItem->statusTip().toInt();
}

void BindAnswerDialog::initDialog(int questionId, int answerId)
{
    questionTableModel->setFilter(qobject_cast<QSqlTableModel *>(mainQuestionTableView->model())->filter());
    select_question_by_id(questionId);
    selectedAnswerId = kInvalidAnswerId;
    for(int row = 0; row < ui->answerListWidget->count(); row++)
    {
        if(ui->answerListWidget->item(row)->statusTip().toInt() == answerId)
            ui->answerListWidget->setCurrentRow(row);
    }
}