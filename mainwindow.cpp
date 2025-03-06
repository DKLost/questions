#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QFont font("文泉驿微米黑",12);
    //font.setStyleStrategy(QFont::PreferAntialias);
    QApplication::setFont(font);
    currentDate = QDate::currentDate();
    questionSql = new QuestionSql("question.db",this);

    init_questionTableView();
    init_answerTableView();

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
    connect(ui->answerListWidget->model(),&QAbstractItemModel::rowsMoved,
            this,&MainWindow::on_answerListRowsMoved);

    learningDialog = new LearningDialog(questionSql,this);                          //init learning dialog
    setTimeDialog = new SetTimeDialog(this);                                        //init set time dialog
    htmlTableAddDialog = new HtmlTableAddDialog(this);                              //init html table add dialog
    questionTagEditDialog = new QuestionTagEditDialog(this);                        //init questionTagEditDialog
    questionMoveDialog = new QuestionMoveDialog(ui->categoryTreeView,this);         //init question move dialog
    bindAnswerDialog = new BindAnswerDialog{questionSql,                            //init bind answer dialog
                                            ui->categoryTreeView,
                                            ui->tagTableView,this};
    answerEditDialog = new AnswerEditDialog(bindAnswerDialog,this);                 //init answer edit dialog

    //init current section
    currentSection = -1;

    //init splitter
    ui->splitter->setSizes({600,250});
}

MainWindow::~MainWindow()
{
    delete ui;
}


//answer
void MainWindow::init_answerTableView()
{
    answerTableModel = new QSqlTableModel(this,questionSql->getDb());
    answerTableModel->setTable("answers");
    answerTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);

    ui->answerTableView->setModel(answerTableModel);
    ui->answerTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    set_answer_tableHeader();
}
void MainWindow::set_answer_tableHeader()
{
    answerTableModel->setHeaderData(1, Qt::Horizontal, "状态");
    answerTableModel->setHeaderData(2, Qt::Horizontal, "平均评分");
    answerTableModel->setHeaderData(3, Qt::Horizontal, "良好用时");
    answerTableModel->setHeaderData(4, Qt::Horizontal, "平均用时");
    answerTableModel->setHeaderData(5, Qt::Horizontal, "最佳用时");
    answerTableModel->setHeaderData(6, Qt::Horizontal, "到期");
    answerTableModel->setHeaderData(7, Qt::Horizontal, "最近学习");
}
void MainWindow::on_answerAddButton_clicked() //新建答案
{
    if(!ui->questionTableView->currentIndex().isValid())
        return;

    int row = ui->answerListWidget->currentRow() + 1;
    ui->answerListWidget->insertItem(row,"");
    ui->answerListWidget->setCurrentRow(row);
    int id = questionSql->get_max_id("answers") + 1;
    questionSql->add_answer(id);
    questionSql->inc_answer_bind_count(id);
    ui->answerListWidget->currentItem()->setStatusTip(QString::number(id));
    ui->answerListWidget->currentItem()->setToolTip("auto");
    ui->answerListWidget->currentItem()->setWhatsThis("请输入答案");
    ui->answerListWidget->currentItem()->setText("请输入答案");
    questionSql->set_value<QString>("answers",id,"goodTime","00m:00s");
    QModelIndex index = ui->questionTableView->currentIndex();
    int qId = index.siblingAtColumn(0).data().toInt();
    save_answerList(qId);
    questionSql->update_question_state(qId);
    on_answerEditButton_clicked();
}
void MainWindow::on_answerEditButton_clicked() //修改答案
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    int qId = index.siblingAtColumn(0).data().toInt();

    QListWidgetItem *item = ui->answerListWidget->currentItem();
    if(item == NULL)
        return;

    //init
    int aId = item->statusTip().toInt();
    QTime goodTime = ToolFunctions::ms2QTime(questionSql->get_value("answers",aId,"goodTime").toString());
    answerEditDialog->setQId(qId);
    answerEditDialog->resetEdit();
    answerEditDialog->setType(item->toolTip());
    answerEditDialog->setGoodTime(goodTime);
    answerEditDialog->setAId(aId);
    if(item->toolTip() == "manual(image)")
    {
        answerEditDialog->setContent(item->whatsThis());
    }else
    {
        answerEditDialog->setContent(item->text());
    }
    answerEditDialog->lineEdit_selectAll();

    //exec
    if(answerEditDialog->exec())
    {
        //return
        QString answerType = answerEditDialog->getRetType();
        QString answerContent = answerEditDialog->getRetContent();
        QString answerGoodTime = answerEditDialog->getRetGoodTime().toString("mm'm':ss's'");
        int answerAId = answerEditDialog->getRetAId();
        // if(answerContent == "")
        //     return;

        if(aId != answerAId)
        {
            item->setStatusTip(QString::number(answerAId));
            questionSql->inc_answer_bind_count(answerAId);
            questionSql->del_answer(aId);
        }

        item->setToolTip(answerType);
        item->setWhatsThis(answerContent);
        if(answerType == "manual(image)")
        {
            item->setIcon(QIcon{answerContent});
            item->setText("");
        }else{
            item->setText(answerContent);
        }
        questionSql->set_value("answers",aId,"goodTime",answerGoodTime);
        questionSql->update_question_state(qId);
        save_answerList(qId);
    }
}
void MainWindow::on_answerDelButton_clicked() //删除答案
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    int qId = index.siblingAtColumn(0).data().toInt();

    QListWidgetItem *item = ui->answerListWidget->currentItem();
    if(item == NULL)
        return;

    int aId = item->statusTip().toInt();
    questionSql->del_answer(aId);
    questionSql->update_question_state(qId);
    delete item;

    save_answerList(qId);
    if(ui->answerListWidget->currentItem() == nullptr)
        answerTableModel->select();
    else
        on_answerListWidget_itemActivated(ui->answerListWidget->currentItem());
}
void MainWindow::save_answerList(int id) //保存指定题目的所有答案
{
    QJsonArray array;

    int rowCount = ui->answerListWidget->count();
    for(int i = 0;i < rowCount;i++)
    {
        QJsonObject object;
        QString type = ui->answerListWidget->item(i)->toolTip();

        object.insert("id",ui->answerListWidget->item(i)->statusTip().toInt());
        object.insert("type",ui->answerListWidget->item(i)->toolTip());
        object.insert("content",ui->answerListWidget->item(i)->whatsThis());

        array.append(object);
    }
    questionSql->write_answerJSON(id,array);
    //answerTableModel->select();
}
void MainWindow::on_answerListWidget_itemDoubleClicked(QListWidgetItem *item)
{
    on_answerEditButton_clicked();
}
void MainWindow::on_answerListRowsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationRow)
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    int id = index.siblingAtColumn(0).data().toInt();
    save_answerList(id);
}
void MainWindow::on_answerListWidget_itemChanged(QListWidgetItem *item)
{
    if(item != nullptr)
    {
        int aId = ui->answerTableView->currentIndex().siblingAtColumn(0).data().toInt();
        if(aId != item->statusTip().toInt())
            on_answerListWidget_itemActivated(item);
    }
}
void MainWindow::on_answerListWidget_itemClicked(QListWidgetItem *item)
{
    if(item != nullptr)
    {
        int aId = ui->answerTableView->currentIndex().siblingAtColumn(0).data().toInt();
        if(aId != item->statusTip().toInt())
            on_answerListWidget_itemActivated(item);
    }
}
void MainWindow::on_answerListWidget_itemActivated(QListWidgetItem *item)
{
    if(item != nullptr)
    {
        int aId = item->statusTip().toInt();
        QString condString = QString("id = %1").arg(aId);
        answerTableModel->setFilter(condString);
        answerTableModel->select();
    }
}

//question
void MainWindow::init_questionTableView()
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
void MainWindow::set_quesion_tableHeader()
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
int MainWindow::select_question_by_id(int id) //选取问题，成功返回1，失败返回0
{
    auto indexList = questionTableModel->match(questionTableModel->index(0,0),Qt::DisplayRole,id);
    if(id < 0 || indexList.isEmpty())
        return 0;
    QModelIndex index = *indexList.begin();
    ui->questionTableView->setCurrentIndex(index);
    on_questionTableView_activated(index);
    return 1;
}
void MainWindow::on_questionDirOpenButton_clicked()
{
    if(ui->questionTableView->currentIndex().isValid())
    {
        int qId = ui->questionTableView->currentIndex().siblingAtColumn(0).data().toInt();
        QString path = QString("%1/data/%2/question.html").arg(QDir::currentPath()).arg(qId);
        QStringList args;
        args << "/select," << QDir::toNativeSeparators(path);
        QProcess process;
        process.startDetached("explorer",args);
    }
}
void MainWindow::on_questionTableView_clicked(const QModelIndex &index)
{
    on_questionTableView_activated(index);
}
void MainWindow::on_questionTableView_pressed(const QModelIndex &index)
{
    //on_questionTableView_activated(index);
}
void MainWindow::on_questionTableView_entered(const QModelIndex &index)
{
    if(QGuiApplication::mouseButtons() == Qt::NoButton)
        return;
    if(QGuiApplication::mouseButtons() == Qt::RightButton)
        return;
    on_questionTableView_activated(index);
}
void MainWindow::on_questionTableView_activated(const QModelIndex &index) //题目选取功能 2024/8/11
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
void MainWindow::on_questionAddButton_clicked()
{
    int id = questionSql->get_max_id("questions") + 1;
    QModelIndex index =ui->categoryTreeView->currentIndex();
    int categoryId = index.siblingAtColumn(1).data().toInt();
    QString name = ui->lineEdit->text();
    questionSql->add_question(id,categoryId,name);

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

    //ui->answerListWidget->addItem("请输入答案");
    save_answerList(id);
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
    questionMoveDialog->setCurrentIndex(ui->categoryTreeView->currentIndex());
    questionMoveDialog->exec();
    int newCategoryId = questionMoveDialog->getRetId();
    if(newCategoryId == 0)
        return;
    int id = 0;

    for(auto index : indexes)
    {
        id = index.siblingAtColumn(0).data().toInt();
        questionSql->set_value("questions",id,"categoryId",newCategoryId);
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
void MainWindow::on_questionRenameButton_clicked()
{
    QModelIndex index = ui->questionTableView->currentIndex();

    questionRenameDialog.setText(index.siblingAtColumn(2).data().toString());
    if(questionRenameDialog.exec())
    {
        questionTableModel->setData(index.siblingAtColumn(2),questionRenameDialog.getText());
        questionTableModel->submitAll();
    }
}

//learn
void MainWindow::on_itemLearnButton_clicked()//单项学习
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    int id = index.siblingAtColumn(0).data().toInt();

    learningDialog->clear_question_display();
    learningDialog->set_items_table(QString("questions.id = %1").arg(id));
    learningDialog->setOnlyToLearn(false);
    learningDialog->exec();

    //update categoryTreeView
    int categoryId =ui->categoryTreeView->currentIndex().siblingAtColumn(1).data().toInt();
    reload_categoryTreeView();
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[categoryId][0]->index());

    questionTableModel->select();
    if(learningDialog->getLastId() != -1)
    {
        select_question_by_id(learningDialog->getLastId());
    }
}
void MainWindow::on_speedLearnButton_clicked()// 标签学习（不重复）
{
    learningDialog->setOnlyToLearn(false);
    learningDialog->setIsSpeedLearn(true);
    learningDialog->set_items_table(questionTableModel->filter());
    learningDialog->exec();
    learningDialog->setIsSpeedLearn(false);

    //update categoryTreeView
    QModelIndex index = ui->categoryTreeView->currentIndex();
    int categoryId = index.siblingAtColumn(1).data().toInt();
    reload_categoryTreeView();
    if(!index.isValid())
        return;
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[categoryId][0]->index());
    questionTableModel->select();
    if(learningDialog->getLastId() != -1)
    {
        select_question_by_id(learningDialog->getLastId());
    }

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
void MainWindow::on_categoryToLearnButton_clicked()//表项学习（仅待学）
{
    learningDialog->clear_question_display();
    learningDialog->set_items_table(questionSql->get_toLearn_condString(questionTableModel->filter()));
    learningDialog->setOnlyToLearn(true);
    learningDialog->exec();

    //update categoryTreeView
    QModelIndex index = ui->categoryTreeView->currentIndex();
    int categoryId = index.siblingAtColumn(1).data().toInt();
    reload_categoryTreeView();
    if(!index.isValid())
        return;
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[categoryId][0]->index());

    questionTableModel->select();
    if(learningDialog->getLastId() != -1)
    {
        select_question_by_id(learningDialog->getLastId());
    }
}
void MainWindow::on_categoryLearnButton_clicked() //表项学习（全部）
{
    learningDialog->set_items_table(questionTableModel->filter());
    learningDialog->setOnlyToLearn(false);
    learningDialog->exec();

    //update categoryTreeView
    QModelIndex index = ui->categoryTreeView->currentIndex();
    int categoryId = index.siblingAtColumn(1).data().toInt();
    reload_categoryTreeView();
    if(!index.isValid())
        return;
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[categoryId][0]->index());

    questionTableModel->select();
    if(learningDialog->getLastId() != -1)
    {
        select_question_by_id(learningDialog->getLastId());
    }
}

//html edit
void MainWindow::on_htmlImgAddButton_clicked() //插入图片功能 2024/8/7
{
    if(ui->questionTableView->currentIndex().isValid())
    {
        int qId = ui->questionTableView->currentIndex().siblingAtColumn(0).data().toInt();
        QString dirPath = QString("%1/data/%2/").arg(QDir::currentPath()).arg(qId);
        QString filePath = QFileDialog::getOpenFileName(this,
                                                        "打开图片", dirPath, tr("Image Files (*.jpg *.png *.gif)"));
        if(filePath.isNull() || filePath.isEmpty() || filePath == "")
            return;
        QDir dir("./");
        QTextCursor cursor = ui->questionTextEdit->textCursor();
        QString imgHtml = QString("<img src=\"./%1\">").arg(dir.relativeFilePath(filePath));
        cursor.insertHtml(imgHtml);
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

//category
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
void MainWindow::update_count_categoryTreeView()
{
    QSqlQuery query(QSqlDatabase::database("connection1"));
    query.prepare("SELECT * FROM categories");
    query.exec();
    while(query.next())
    {
        int id = query.value(0).toInt();
        if(id == 0)
            continue;
        int qCount = questionSql->count_total_questions(id);
        int qtoLearnCount = questionSql->count_total_questions_to_learn(id);
        categoryItemLists[id][2]->setText(QString::number(qtoLearnCount));
        categoryItemLists[id][3]->setText(QString::number(qCount));
    }
}
void MainWindow::get_categoryItemTree(QStandardItem* parent,int parentId)
{
    QSqlQuery query(QSqlDatabase::database("connection1"));
    query.prepare("SELECT * FROM categories WHERE parentId = ? ORDER BY name");
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

    int id = questionSql->get_max_id("categories") + 1;
    QModelIndex index =ui->categoryTreeView->currentIndex();
    QString name = ui->lineEdit->text();
    int parentId = index.siblingAtColumn(1).data().toInt();
    if(parentId == 1)
        parentId = 0;

    QSqlQuery query(QSqlDatabase::database("connection1"));
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
    if(ui->tagTableView->currentIndex().isValid())
        ui->tagTableView->selectionModel()->clearCurrentIndex();
    ui->tagTableView->clearSelection();
    if(currentDate != QDate::currentDate())
    {
        update_count_categoryTreeView();
        currentDate = QDate::currentDate();
    }

    int categoryId = index.siblingAtColumn(1).data().toInt();
    QString condString = questionSql->get_category_condString(categoryId);
    questionTableModel->setFilter(condString);
    currentSection = 0;
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
        questionSql->set_value("categories",id,"name",newName);
    }
    on_categoryTreeView_clicked(item->index());
}

//tag
void MainWindow::on_addTagButton_clicked()
{
    int id = questionSql->get_max_id("tags") + 1;
    QString name = ui->lineEdit->text();
    QSqlQuery query(QSqlDatabase::database("connection1"));
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

//other
void MainWindow::on_setFontButton_clicked()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, QFont("黑体", 12), this);
    if(ok)
        ui->questionTextEdit->setCurrentFont(font);
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
void MainWindow::on_setGoodTimeButton_clicked()
{
    QModelIndexList indexes = ui->questionTableView->selectionModel()->selectedIndexes();
    if(indexes.count() <= 0)
        return;
    int firstId = indexes.begin()->siblingAtColumn(0).data().toInt();
    int id = firstId;
    QTime goodTime = QTime::fromString(questionSql->get_value("questions",id,"goodTime").toString(),"mm'm':ss's'");
    setTimeDialog->setTime(goodTime);
    setTimeDialog->exec();
    QTime time = setTimeDialog->getTime();
    if(time.msecsSinceStartOfDay() == 0)
        return;

    for(auto index : indexes)
    {
        id = index.siblingAtColumn(0).data().toInt();
        QJsonArray answerArray = questionSql->read_answerJSON(id);
        time = QTime::fromMSecsSinceStartOfDay(time.msecsSinceStartOfDay()/answerArray.count()/1000*1000);
        for(auto a : answerArray)
        {
            int aId = a.toObject().value("id").toInt();
            questionSql->set_value("answers",aId,"goodTime",time.toString("mm'm':ss's'"));
        }
        time =  QTime::fromMSecsSinceStartOfDay(time.msecsSinceStartOfDay()*answerArray.count());
        questionSql->set_value("questions",id,"goodTime",time.toString("mm'm':ss's'"));
    }

    questionTableModel->select();
    QModelIndex newIndex = *questionTableModel->match(questionTableModel->index(0,0),Qt::DisplayRole,firstId).begin();
    ui->questionTableView->setCurrentIndex(newIndex);
    on_questionTableView_activated(newIndex);
}












