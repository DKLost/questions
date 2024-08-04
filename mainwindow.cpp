#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    questionSql = new QuestionSql("question.db",this);

    //init questionTableView
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
    set_tableHeader();
    ui->questionTableView->installEventFilter(this);

    //init categoryTreeView
    categoryItemModel = new QStandardItemModel(this);
    reload_categoryTreeView();
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[1][0]->index());
    on_categoryTreeView_clicked(categoryItemLists[1][0]->index());
    connect(categoryItemModel,&QStandardItemModel::itemChanged,this,&MainWindow::category_item_change_handler);

    //init tagTableView
    tagTableModel = new QSqlTableModel(this,questionSql->getDb());
    tagTableModel->setTable("tags");
    tagTableModel->setHeaderData(1,Qt::Horizontal,"标签名称");
    tagTableModel->setHeaderData(2,Qt::Horizontal,"最佳用时");
    tagTableModel->select();
    ui->tagTableView->setModel(tagTableModel);
    ui->tagTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tagTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tagTableView->hideColumn(0);
    ui->tagTableView->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    ui->tagTableView->setSortingEnabled(true);

    //init question html text edit
    ui->questionTextEdit->setTabStopDistance(ui->questionTextEdit->fontMetrics().horizontalAdvance(' ')*4);

    //init answer list
    ui->answerListWidget->setMovement(QListView::Free);
    ui->answerListWidget->setDragDropMode(QAbstractItemView::InternalMove);

    //init question move dialog
    questionMoveDialog = new QuestionMoveDialog(categoryItemModel,this);

    //init answer edit dialog
    answerEditDialog = new AnswerEditDialog(this);

    //init learning dialog
    learningDialog = new LearningDialog(questionSql,this);

    //init set time dialog
    setTimeDialog = new SetTimeDialog(this);

    //init html table add dialog
    htmlTableAddDialog = new HtmlTableAddDialog(this);

    //init questionTagEditDialog
    questionTagEditDialog = new QuestionTagEditDialog(this);

    //init current section
    currentSection = -1;

    //init splitter
    ui->splitter->setSizes({250,600,350});
    ui->splitter->setStretchFactor(0,0);
    ui->splitter->setStretchFactor(1,0);
    ui->splitter->setStretchFactor(2,1);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::set_tableHeader()
{
    questionTableModel->setHeaderData(1, Qt::Horizontal, "类别");
    questionTableModel->setHeaderData(2, Qt::Horizontal, "状态");
    questionTableModel->setHeaderData(3, Qt::Horizontal, "平均评分");
    questionTableModel->setHeaderData(4, Qt::Horizontal, "平均用时");
    questionTableModel->setHeaderData(5, Qt::Horizontal, "良好用时");
    questionTableModel->setHeaderData(6, Qt::Horizontal, "最佳用时");
    questionTableModel->setHeaderData(7, Qt::Horizontal, "到期");
    questionTableModel->setHeaderData(8, Qt::Horizontal, "最近学习");
    for(int i = 9;i < 21;i++)
    {
        ui->questionTableView->hideColumn(i);
    }
}

void MainWindow::reload_categoryTreeView()
{
    categoryItemLists.clear();
    categoryItemModel->clear();
    categoryItemModel->setHorizontalHeaderLabels({"名称","id","待学习","总数"});
    categoryItemLists[0].append(categoryItemModel->invisibleRootItem());
    get_categoryItemTree(categoryItemLists[0][0],0);
    ui->categoryTreeView->setModel(categoryItemModel);
    ui->categoryTreeView->header()->setStretchLastSection(false);
    ui->categoryTreeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->categoryTreeView->header()->setSectionResizeMode(0,QHeaderView::Stretch);
    ui->categoryTreeView->hideColumn(1);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
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
        return QMainWindow::eventFilter(watched, event);
    }
}

void MainWindow::get_categoryItemTree(QStandardItem* parent,int parentId)
{
    QSqlQuery query(QSqlDatabase::database("connection1"));
    query.prepare("SELECT * FROM categories WHERE parentId = ?");
    query.bindValue(0,parentId);
    query.exec();
    while(query.next())
    {
        int id = query.value(0).toInt();
        QString name = query.value(1).toString();
        int qCount = questionSql->count_total_questions(id);
        int qtoLearnCount = questionSql->count_total_questions_to_learn(id);
        categoryItemLists[id].append(new QStandardItem(name));
        categoryItemLists[id].append(new QStandardItem(QString::number(id)));
        categoryItemLists[id].append(new QStandardItem(QString::number(qtoLearnCount)));
        categoryItemLists[id].append(new QStandardItem(QString::number(qCount)));
        parent->appendRow(categoryItemLists[id]);
        get_categoryItemTree(categoryItemLists[id][0],id);
    }
}

void MainWindow::on_categoryAddButton_clicked()
{
    QSqlQuery query(QSqlDatabase::database("connection1"));
    QModelIndex index =ui->categoryTreeView->currentIndex();
    query.exec("SELECT MAX(id) FROM categories");
    query.next();
    int id = query.value(0).toInt() + 1;
    QString name = ui->lineEdit->text();
    int parentId = index.siblingAtColumn(1).data().toInt();
    if(parentId == 1)
        parentId = 0;
    query.prepare("INSERT INTO categories VALUES(?,?,?)");
    query.bindValue(0,id);
    query.bindValue(1,name);
    query.bindValue(2,parentId);
    query.exec();
    reload_categoryTreeView();
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[parentId][0]->index());
}

void MainWindow::on_categoryDelButton_clicked()
{
    QModelIndex index =ui->categoryTreeView->currentIndex();
    int id = index.siblingAtColumn(1).data().toInt();
    if(id == 1)
        return;
    questionSql->del_category(id);
    QStandardItem *parent = categoryItemLists[id][0]->parent();
    if(!parent)
        parent = categoryItemLists[0][0];
    int row = categoryItemLists[id][0]->row();
    parent->removeRow(row);

    int parentId = parent->index().siblingAtColumn(1).data().toInt();
    if(parentId == 0)
        parentId = 1;
    reload_categoryTreeView();
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[parentId][0]->index());
    on_categoryTreeView_clicked(categoryItemLists[parentId][0]->index());
}

void MainWindow::on_categoryTreeView_clicked(const QModelIndex &index)
{
    int categoryId = index.siblingAtColumn(1).data().toInt();
    QString condString = questionSql->get_category_condString(categoryId);
    questionTableModel->setFilter(condString);
    currentSection = 0;
}

void MainWindow::on_questionAddButton_clicked()
{
    QSqlQuery query(QSqlDatabase::database("connection1"));
    QModelIndex index =ui->categoryTreeView->currentIndex();
    query.exec("SELECT MAX(id) FROM questions");

    int id = 1;
    if(query.next())
        id = query.value(0).toInt() + 1;

    int categoryId = index.siblingAtColumn(1).data().toInt();
    questionSql->add_question(id,categoryId);

    //reload categoryTreeView
    reload_categoryTreeView();
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[categoryId][0]->index());
    on_categoryTreeView_clicked(categoryItemLists[categoryId][0]->index());

    //select new item
    int row = 0;
    for(;row < questionTableModel->rowCount();row++)
    {
        int _id = questionTableModel->index(row,0).data().toInt();
        if(id == _id)
            break;
    }
    ui->questionTableView->selectRow(row);
    on_questionTableView_clicked(ui->questionTableView->currentIndex());

    ui->answerListWidget->addItem("请输入答案");
    save_answerList(id);
}

void MainWindow::on_questionTextEdit_textChanged()
{
    if(!is_questionTextEdit_editable)
        return;
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    int id = index.siblingAtColumn(0).data().toInt();
    QString questionHTML;
    questionHTML = ui->questionTextEdit->toHtml();
    questionSql->write_questionHTML(id,questionHTML);
    on_categoryTreeView_clicked(ui->categoryTreeView->currentIndex());
    ui->questionTableView->selectRow(index.row());
}


void MainWindow::on_questionTableView_clicked(const QModelIndex &index)
{
    on_questionTableView_activated(index);
}

void MainWindow::on_questionTableView_activated(const QModelIndex &index)
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
    for(auto item : array)
    {
        ui->answerListWidget->addItem(item.toString());
    }
}

void MainWindow::on_questionDelButton_clicked()
{
    QModelIndex index = ui->questionTableView->currentIndex();
    int id = index.siblingAtColumn(0).data().toInt();
    ui->questionTableView->clearSelection();
    ui->questionTextEdit->setText("");
    questionSql->del_question(id);

    int row = ui->questionTableView->currentIndex().row();
    int categoryId =ui->categoryTreeView->currentIndex().siblingAtColumn(1).data().toInt();
    reload_categoryTreeView();
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[categoryId][0]->index());
    on_categoryTreeView_clicked(categoryItemLists[categoryId][0]->index());

    //reset table view
    if(questionTableModel->rowCount() > 0)
    {
        if(row >= questionTableModel->rowCount())
            row = questionTableModel->rowCount() - 1;
        ui->questionTableView->selectRow(row);
        on_questionTableView_clicked(ui->questionTableView->currentIndex());
    }
}

void MainWindow::on_questionMoveButton_clicked()
{
    QModelIndexList indexes = ui->questionTableView->selectionModel()->selectedIndexes();

    //get new category id
    questionMoveDialog->setRetId(0);
    questionMoveDialog->exec();
    int newCategoryId = questionMoveDialog->getRetId();
    if(newCategoryId == 0)
        return;
    int id = 0;

    for(auto index : indexes)
    {
        id = index.siblingAtColumn(0).data().toInt();
        questionSql->set_question_categoryId(id,newCategoryId);
    }

    reload_categoryTreeView();
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[newCategoryId][0]->index());
    on_categoryTreeView_clicked(categoryItemLists[newCategoryId][0]->index());
    QAbstractItemModel *model = ui->questionTableView->model();
    QModelIndex newIndex;
    for(int i = 0;i < model->rowCount();i++)
    {
        newIndex = model->index(i,0);
        if(newIndex.data().toInt() == id)
            break;
    }
    ui->questionTableView->setCurrentIndex(newIndex);
    on_questionTableView_activated(newIndex);
}

void MainWindow::on_answerAddButton_clicked()
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    int id = index.siblingAtColumn(0).data().toInt();

    answerEditDialog->setRetString("");
    answerEditDialog->clearTextEdit();
    answerEditDialog->lineEdit_selectAll();
    answerEditDialog->exec();

    QString answerString = answerEditDialog->getRetString();
    if(answerString == "")
        return;

    int row = ui->answerListWidget->currentRow() + 1;
    ui->answerListWidget->insertItem(row,answerString);
    ui->answerListWidget->setCurrentRow(row);
    save_answerList(id);
}


void MainWindow::on_answerDelButton_clicked()
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    int id = index.siblingAtColumn(0).data().toInt();

    QListWidgetItem *item = ui->answerListWidget->currentItem();
    if(item == NULL)
        return;

    delete item;

    save_answerList(id);
}

void MainWindow::on_answerEditButton_clicked()
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    int id = index.siblingAtColumn(0).data().toInt();

    QListWidgetItem *item = ui->answerListWidget->currentItem();
    if(item == NULL)
        return;

    answerEditDialog->setRetString("");
    answerEditDialog->setTextEdit(item->text());
    answerEditDialog->lineEdit_selectAll();
    answerEditDialog->exec();

    QString answerString = answerEditDialog->getRetString();
    if(answerString == "")
        return;
    item->setText(answerString);

    save_answerList(id);
}

void MainWindow::on_answerListWidget_itemDoubleClicked(QListWidgetItem *item)
{
    on_answerEditButton_clicked();
}

void MainWindow::save_answerList(int id)
{
    QJsonArray array;
    int rowCount = ui->answerListWidget->count();
    for(int i = 0;i < rowCount;i++)
    {
        array.append(ui->answerListWidget->item(i)->text());
    }
    questionSql->write_answerJSON(id,array);
}


void MainWindow::on_setGoodTimeButton_clicked()
{
    QModelIndexList indexes = ui->questionTableView->selectionModel()->selectedIndexes();
    if(indexes.count() <= 0)
        return;
    int id = indexes.begin()->siblingAtColumn(0).data().toInt();
    QTime goodTime = QTime::fromString(questionSql->get_data(id,"goodTime").toString(),"mm'm':ss's'");
    setTimeDialog->setTime(goodTime);
    setTimeDialog->exec();
    QTime time = setTimeDialog->getTime();
    if(time.msecsSinceStartOfDay() == 0)
        return;

    for(auto index : indexes)
    {
        id = index.siblingAtColumn(0).data( ).toInt();
        questionSql->set_goodTime(id,time);
    }

    QModelIndex index2 = ui->categoryTreeView->currentIndex();
    on_categoryTreeView_clicked(index2);
    ui->questionTableView->selectRow(index2.row());
}

//单项学习
void MainWindow::on_itemLearnButton_clicked()
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    int id = index.siblingAtColumn(0).data().toInt();

    learningDialog->clear_question_display();
    learningDialog->set_items_table(QString("questions.id = %1").arg(id));
    learningDialog->exec();

    //reload categoryTreeView
    int categoryId =ui->categoryTreeView->currentIndex().siblingAtColumn(1).data().toInt();
    reload_categoryTreeView();
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[categoryId][0]->index());

    //on_categoryTreeView_clicked(categoryItemLists[categoryId][0]->index());
    questionTableModel->select();
}

//类别学习（全部）
void MainWindow::on_categoryLearnButton_clicked()
{
    // learningDialog->clear_question_display();
    // learningDialog->set_itmes_table(questionSql->get_category_condString(categoryId));
    learningDialog->set_items_table(questionTableModel->filter());
    learningDialog->exec();

    //reload categoryTreeView
    QModelIndex index = ui->categoryTreeView->currentIndex();
    int categoryId = index.siblingAtColumn(1).data().toInt();
    reload_categoryTreeView();
    if(!index.isValid())
        return;
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[categoryId][0]->index());

    //on_categoryTreeView_clicked(categoryItemLists[categoryId][0]->index());
    questionTableModel->select();
}

//类别学习（仅待学）
void MainWindow::on_categoryToLearnButton_clicked()
{
    learningDialog->clear_question_display();
    //learningDialog->set_itmes_table(questionSql->get_toLearn_condString(categoryId));
    learningDialog->set_items_table(questionSql->get_toLearn_condString(questionTableModel->filter()));
    learningDialog->exec();

    //reload categoryTreeView
    QModelIndex index = ui->categoryTreeView->currentIndex();
    int categoryId = index.siblingAtColumn(1).data().toInt();
    reload_categoryTreeView();
    if(!index.isValid())
        return;
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[categoryId][0]->index());

    //on_categoryTreeView_clicked(categoryItemLists[categoryId][0]->index());
    questionTableModel->select();
}

// 标签学习（不重复）
void MainWindow::on_speedLearnButton_clicked()
{
    learningDialog->setIsSpeedLearn(true);
    learningDialog->set_items_table(questionTableModel->filter());
    learningDialog->exec();
    learningDialog->setIsSpeedLearn(false);

    //reload categoryTreeView
    QModelIndex index = ui->categoryTreeView->currentIndex();
    int categoryId = index.siblingAtColumn(1).data().toInt();
    reload_categoryTreeView();
    if(!index.isValid())
        return;
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[categoryId][0]->index());
    questionTableModel->select();

    //update tag bestTime
    if(questionTableModel->filter().indexOf("tag") != -1)
    {
        QModelIndex tagTimeIndex = ui->tagTableView->currentIndex().siblingAtColumn(2);
        QString bestTime = tagTimeIndex.data().toString();
        QString newTime = learningDialog->getTotalTime().toString("hh:mm:ss.zzz").chopped(1);
        if(bestTime > newTime)
        {
            tagTableModel->setData(tagTimeIndex,newTime);
            tagTableModel->submitAll();
        }
    }
}

void MainWindow::on_htmlTableAddButton_clicked()
{
    htmlTableAddDialog->setRetRow(0);
    htmlTableAddDialog->setRetColumn(0);
    htmlTableAddDialog->exec();
    QTextTableFormat tf;
    tf.setBorder(1);
    tf.setCellPadding(5);
    int row = htmlTableAddDialog->getRetRow();
    int column = htmlTableAddDialog->getRetColumn();
    ui->questionTextEdit->textCursor().insertTable(row,column,tf);
}


void MainWindow::on_categoryEditButton_clicked()
{
    QModelIndex index = ui->categoryTreeView->currentIndex();
    ui->categoryTreeView->edit(index);
}

void MainWindow::category_item_change_handler(QStandardItem *item)
{
    if(item->column() == 0)
    {
        QString newName = item->index().siblingAtColumn(0).data().toString();
        int id = item->index().siblingAtColumn(1).data().toInt();
        questionSql->set_category_data(id,"name",newName);
    }
    on_categoryTreeView_clicked(item->index());
}


void MainWindow::on_addTagButton_clicked()
{
    QSqlQuery query(QSqlDatabase::database("connection1"));
    query.exec("SELECT MAX(id) FROM tags");
    query.next();
    int id = query.value(0).toInt() + 1;
    QString name = ui->lineEdit->text();

    query.prepare("SELECT * FROM tags WHERE name = ?");
    query.bindValue(0,name);
    query.exec();
    if(query.next())
        return;

    questionSql->add_tag(id,name);
    tagTableModel->select();
}


void MainWindow::on_delTagButton_clicked()
{
    QModelIndex index = ui->tagTableView->currentIndex();
    int id = index.siblingAtColumn(0).data().toInt();
    questionSql->del_tag(id);
    tagTableModel->select();
}


void MainWindow::on_questionTagButton_clicked()
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;

    int qId = index.siblingAtColumn(0).data().toInt();
    questionTagEditDialog->set_question(qId);
    questionTagEditDialog->exec();

    questionTableModel->select();
    tagTableModel->select();
}


void MainWindow::on_tagTableView_activated(const QModelIndex &index)
{
    QSqlQuery query(QSqlDatabase::database("connection1"));
    int tagId = index.siblingAtColumn(0).data().toInt();
    QString condString = QString("(tag GLOB '*,%1') OR (tag GLOB '*,%1,*') OR (tag GLOB '%1,*') OR (tag GLOB '%1')").arg(tagId);
    questionTableModel->setFilter(condString);
    currentSection = 1;
}


void MainWindow::on_tagTableView_clicked(const QModelIndex &index)
{
    on_tagTableView_activated(index);
}

