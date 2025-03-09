#include "bindanswerdialog.h"
#include "ui_bindanswerdialog.h"

BindAnswerDialog::BindAnswerDialog(QuestionSql *mainQuestionSql,QTreeView *mainCategoryTreeView,
                                   QTableView *mainTagTableView,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BindAnswerDialog)
{
    ui->setupUi(this);
    //QFont font("文泉驿微米黑",12);
    //QApplication::setFont(font);
    questionSql = mainQuestionSql;

    init_questionTableView();
    init_answerTableView();

    //init categoryTreeView
    this->mainCategoryTreeView = mainCategoryTreeView;
    ui->categoryTreeView->setModel(mainCategoryTreeView->model());
    ui->categoryTreeView->header()->setStretchLastSection(false);
    ui->categoryTreeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->categoryTreeView->header()->setSectionResizeMode(0,QHeaderView::Stretch);
    ui->categoryTreeView->hideColumn(1);

    //init tagTableView
    this->mainTagTableView = mainTagTableView;
    // tagTableModel = new QSqlTableModel(this,questionSql->getDb());
    // tagTableModel->setTable("tags");
    // tagTableModel->setHeaderData(1,Qt::Horizontal,"标签名称");
    // tagTableModel->setHeaderData(2,Qt::Horizontal,"最佳用时");
    // tagTableModel->select();

    ui->tagTableView->setModel(mainTagTableView->model());
    ui->tagTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tagTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tagTableView->hideColumn(0);
    ui->tagTableView->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    ui->tagTableView->setSortingEnabled(true);

    //init question html text edit
    ui->questionTextEdit->setTabStopDistance(ui->questionTextEdit->fontMetrics().horizontalAdvance(' ')*4);

    //init answer list

    //init splitter
    ui->splitter->setSizes({250,600,350});
    ui->splitter->setStretchFactor(0,0);
    ui->splitter->setStretchFactor(1,0);
    ui->splitter->setStretchFactor(2,1);
}

BindAnswerDialog::~BindAnswerDialog()
{
    delete ui;
}

//answer
void BindAnswerDialog::init_answerTableView()
{
    answerTableModel = new QSqlTableModel(this,questionSql->getDb());
    answerTableModel->setTable("answers");
    answerTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);

    ui->answerTableView->setModel(answerTableModel);
    ui->answerTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    set_answer_tableHeader();
}
void BindAnswerDialog::set_answer_tableHeader()
{
    answerTableModel->setHeaderData(1, Qt::Horizontal, "状态");
    answerTableModel->setHeaderData(2, Qt::Horizontal, "平均评分");
    answerTableModel->setHeaderData(3, Qt::Horizontal, "良好用时");
    answerTableModel->setHeaderData(4, Qt::Horizontal, "平均用时");
    answerTableModel->setHeaderData(5, Qt::Horizontal, "最佳用时");
    answerTableModel->setHeaderData(6, Qt::Horizontal, "到期");
    answerTableModel->setHeaderData(7, Qt::Horizontal, "最近学习");
}
void BindAnswerDialog::on_answerListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    if(current != nullptr)
    {
        int aId = current->statusTip().toInt();
        QString condString = QString("id = %1").arg(aId);
        answerTableModel->setFilter(condString);
        answerTableModel->select();
    }

}

//question
void BindAnswerDialog::init_questionTableView()
{
    questionTableModel = new QSqlRelationalTableModel(this,questionSql->getDb());
    questionTableModel->setTable("questions");
    questionTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    questionTableModel->setRelation(1,QSqlRelation{"categories","id","name"});
    questionTableModel->select();

    ui->questionTableView->setModel(questionTableModel);
    ui->questionTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->questionTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->questionTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->questionTableView->setSortingEnabled(true);
    ui->questionTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    set_quesion_tableHeader();
    ui->questionTableView->installEventFilter(this);
}
void BindAnswerDialog::set_quesion_tableHeader()
{
    questionTableModel->setHeaderData(1, Qt::Horizontal, "类别");
    questionTableModel->setHeaderData(2, Qt::Horizontal, "名称");
    questionTableModel->setHeaderData(3, Qt::Horizontal, "状态");
    questionTableModel->setHeaderData(4, Qt::Horizontal, "平均评分");
    questionTableModel->setHeaderData(5, Qt::Horizontal, "良好用时");
    questionTableModel->setHeaderData(6, Qt::Horizontal, "平均用时");
    questionTableModel->setHeaderData(7, Qt::Horizontal, "最佳用时");
    questionTableModel->setHeaderData(8, Qt::Horizontal, "到期");
    questionTableModel->setHeaderData(9, Qt::Horizontal, "最近学习");
}
void BindAnswerDialog::select_question_by_id(int id)
{
    if(id >= 0)
    {
        auto indexList = questionTableModel->match(questionTableModel->index(0,0),Qt::DisplayRole,id);
        QModelIndex index = *indexList.begin();
        ui->questionTableView->setCurrentIndex(index);
        on_questionTableView_activated(index);
    }
}
void BindAnswerDialog::on_questionTableView_clicked(const QModelIndex &index)
{
    on_questionTableView_activated(index);
}
void BindAnswerDialog::on_questionTableView_pressed(const QModelIndex &index)
{
    //on_questionTableView_activated(index);
}
void BindAnswerDialog::on_questionTableView_entered(const QModelIndex &index)
{
    if(QGuiApplication::mouseButtons() == Qt::NoButton)
        return;
    if(QGuiApplication::mouseButtons() == Qt::RightButton)
        return;
    on_questionTableView_activated(index);
}
void BindAnswerDialog::on_questionTableView_activated(const QModelIndex &index) //题目选取功能 2024/8/11
{
    int id = index.siblingAtColumn(0).data().toInt();

    //load question.html
    QString questionHTML = questionSql->read_questionHTML(id);
    is_questionTextEdit_editable = false;
    ui->questionTextEdit->setHtml(questionHTML);
    is_questionTextEdit_editable = true;

    //load answer.json
    QJsonArray array = questionSql->read_answerJSON(id);
    ui->answerListWidget->clear();
    ui->answerListWidget->setIconSize(QSize(1000,1000));
    for(auto item : array)
    {
        QJsonObject itemObject = item.toObject();
        QString type = itemObject["type"].toString();
        QListWidgetItem *aitem = new QListWidgetItem{};//id:StatusTip,type:ToolTip,content:WhatsThis
        aitem->setStatusTip(QString::number(itemObject["id"].toInt()));
        aitem->setToolTip(type);
        aitem->setWhatsThis(itemObject["content"].toString());
        if(type == "manual(image)")
        {
            QIcon icon{itemObject["content"].toString()};
            aitem->setIcon(icon);
        }else
            aitem->setText(itemObject["content"].toString());
        ui->answerListWidget->addItem(aitem);
    }
}

//category
void BindAnswerDialog::on_categoryTreeView_clicked(const QModelIndex &index)
{
    if(ui->tagTableView->currentIndex().isValid())
        ui->tagTableView->selectionModel()->clearCurrentIndex();
    ui->tagTableView->clearSelection();

    int categoryId = index.siblingAtColumn(1).data().toInt();
    QString condString = questionSql->get_category_condString(categoryId);
    questionTableModel->setFilter(condString);
}

//tag
void BindAnswerDialog::on_tagTableView_activated(const QModelIndex &index)
{
    QSqlQuery query(QSqlDatabase::database("connection1"));
    int tagId = index.siblingAtColumn(0).data().toInt();
    QString condString = QString("(tag GLOB '*,%1') OR (tag GLOB '*,%1,*') OR (tag GLOB '%1,*') OR (tag GLOB '%1')").arg(tagId);
    questionTableModel->setFilter(condString);
}
void BindAnswerDialog::on_tagTableView_clicked(const QModelIndex &index)
{
    on_tagTableView_activated(index);
}

//other
bool BindAnswerDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->questionTableView) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if(keyEvent->key() == Qt::Key_Up)
            {
                QModelIndex index = ui->questionTableView->currentIndex();
                if(index.row() > 0)
                {
                    ui->questionTableView->selectRow(index.row() - 1);
                    ui->questionTableView->activated(ui->questionTableView->currentIndex());
                }
            }else if(keyEvent->key() == Qt::Key_Down)
            {
                QModelIndex index = ui->questionTableView->currentIndex();
                if(index.row() < ui->questionTableView->model()->rowCount())
                {
                    ui->questionTableView->selectRow(index.row() + 1);
                    ui->questionTableView->activated(ui->questionTableView->currentIndex());
                }
            }
            return true;
        } else {
            return false;
        }
    } else {
        // pass the event on to the parent class
        return BindAnswerDialog::eventFilter(watched, event);
    }
}

int BindAnswerDialog::getRetAId() const
{
    return retAId;
}

void BindAnswerDialog::setRetAId(int newRetAId)
{
    retAId = newRetAId;
}

//return
void BindAnswerDialog::on_buttonBox_accepted()
{
    QListWidgetItem *currentItem = ui->answerListWidget->currentItem();
    if(currentItem == nullptr)
        retAId = -1;
    else
        retAId = currentItem->statusTip().toInt();
}

//init
void BindAnswerDialog::initDialog(int qId,int aId)
{
    ui->categoryTreeView->setCurrentIndex(mainCategoryTreeView->currentIndex());
    if(mainTagTableView->currentIndex().isValid())
    {
        ui->tagTableView->setCurrentIndex(mainTagTableView->currentIndex());
        on_tagTableView_clicked(ui->tagTableView->currentIndex());
    }
    else
        on_categoryTreeView_clicked(ui->categoryTreeView->currentIndex());
    retAId = -1;

    select_question_by_id(qId);
    for(int i = 0;i < ui->answerListWidget->count();i++)
    {
        if(ui->answerListWidget->item(i)->statusTip().toInt() == aId)
            ui->answerListWidget->setCurrentRow(i);
    }
}
