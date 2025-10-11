#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QFont font({"IBM Plex Mono", "IBM Plex Math", "Noto Sans Mono", "Microsoft YaHei UI", "Microsoft YaHei", "Arial"},12);
    QApplication::setFont(font);
    currentDate = QDate::currentDate();
    questionSql = new QuestionSql("question.db",this);
    
    init_questionTableView();
    init_answerTableView();
    init_answerTreeWidget();

    //init categoryTreeView
    categoryItemModel = new QStandardItemModel(this);
    reload_categoryTreeView();
    ui->categoryTreeView->setCurrentIndex(categoryItemLists[1][0]->index());
    on_categoryTreeView_clicked(categoryItemLists[1][0]->index());
    connect(categoryItemModel,&QStandardItemModel::itemChanged,this,&MainWindow::category_item_change_handler);

    //init question html text edit
    ui->questionTextEdit->setTabStopDistance(ui->questionTextEdit->fontMetrics().horizontalAdvance(' ')*4);
    ui->questionTextEdit->document()->setIndentWidth(32.5); // 固定缩进值为32.5 2025/10/3
    ui->questionTextEdit->setFont(font);

    //init dialogs
    learningDialog = new LearningDialog(questionSql,this);                          //init learning dialog
    setTimeDialog = new SetTimeDialog(this);                                        //init set time dialog
    htmlTableAddDialog = new HtmlTableAddDialog(this);                              //init html table add dialog
    questionMoveDialog = new QuestionMoveDialog(ui->categoryTreeView,this);         //init question move dialog
    bindAnswerDialog = new BindAnswerDialog{questionSql,                            //init bind answer dialog
                                            ui->categoryTreeView,
                                            this};
    answerEditDialog = new AnswerEditDialog(bindAnswerDialog,this);                 //init answer edit dialog
    descAddDialog = new DescAddDialog();

    //init current section
    currentSection = -1;

    //init splitter
    ui->splitter->setSizes({600,250});

    //init qIdHistoryPos
    qIdHistoryPos = 0;
}

MainWindow::~MainWindow()
{
    delete ui;
}

//answer
void MainWindow::load_answer(int qId)
{
    //load answer.json
    QJsonArray array = questionSql->read_answerJSON(qId);
    ui->answerTreeWidget->clear();
    ui->answerTreeWidget->setIconSize(QSize(1000,1000));
    for(int row = 0;row < array.count();row++)
    {
        QJsonObject itemObject = array[row].toObject();
        QString type = itemObject["type"].toString();
        QTreeWidgetItem *aitem = new QTreeWidgetItem(ui->answerTreeWidget);//id:StatusTip,type:ToolTip,content:WhatsThis
        aitem->setText(0,QString::number(row+1));
        if(type == "manual(image)")
        {
            QIcon icon{itemObject["content"].toString()};
            aitem->setIcon(1,icon);
        }else if(type == "auto(typst)") //添加typst数学公式显示功能 25/10/11
        {
            QString imgPath = QString("./data/%1/%2.png").arg(qId).arg(itemObject["id"].toInt());
            QIcon icon{imgPath};
            aitem->setIcon(1,icon);
        }else
            aitem->setText(1,itemObject["content"].toString());
        aitem->setText(2,QString::number(itemObject["pool"].toInt()));
        ui->answerTreeWidget->addTopLevelItem(aitem);
    }
}

void MainWindow::init_answerTableView()
{
    answerTableModel = new QSqlTableModel(this,questionSql->getDb());
    answerTableModel->setTable("constructs");
    answerTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);

    ui->answerTableView->setModel(answerTableModel);
    ui->answerTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    set_answer_tableHeader();
}

void MainWindow::set_answer_tableHeader()
{
    answerTableModel->setHeaderData(5, Qt::Horizontal, "状态");
    answerTableModel->setHeaderData(6, Qt::Horizontal, "平均评分");
    answerTableModel->setHeaderData(7, Qt::Horizontal, "良好用时");
    answerTableModel->setHeaderData(8, Qt::Horizontal, "平均用时");
    answerTableModel->setHeaderData(9, Qt::Horizontal, "最佳用时");
    answerTableModel->setHeaderData(10, Qt::Horizontal, "到期");
    answerTableModel->setHeaderData(11, Qt::Horizontal, "最近学习");
}

void MainWindow::init_answerTreeWidget()
{
    //ui->answerTreeWidget->setHeaderLabels({"编号","内容"});
    ui->answerTreeWidget->setDragDropMode(QAbstractItemView::InternalMove);
    ui->answerTreeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->answerTreeWidget->headerItem()->setHidden(false);
    ui->answerTreeWidget->setIndentation(0);
    ui->answerTreeWidget->setHeaderLabels({"num","content","pool"});
    //ui->answerTreeWidget->resizeColumnToContents(0);
    ui->answerTreeWidget->header()->setStretchLastSection(false);
    ui->answerTreeWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->answerTreeWidget->header()->setSectionResizeMode(1,QHeaderView::Stretch);

    connect(ui->answerTreeWidget,&DragQTreeWidget::signalRowsMoved,
            this,&MainWindow::answerItemRowsMoved);
}

void MainWindow::on_answerTreeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    //int qId = index.siblingAtColumn(0).data().toInt();
    int qId = currentQId;

    if(item == NULL)
        return;

    //init
    int row = item->text(0).toInt() - 1;
    QJsonArray array = questionSql->read_answerJSON(qId);
    QJsonObject aObj = array[row].toObject();

    int aId = aObj["id"].toInt();
    QTime goodTime = ToolFunctions::ms2QTime(questionSql->get_value("constructs",aId,"goodTime").toString());
    //QString type = aObj["type"].toString();
    //QString content = aObj["content"].toString();
    answerEditDialog->setQId(qId);
    answerEditDialog->resetEdit();
    answerEditDialog->setGoodTime(goodTime);
    answerEditDialog->setAId(aId);
    answerEditDialog->setType(aObj["type"].toString());
    answerEditDialog->setPool(aObj["pool"].toInt());
    answerEditDialog->setContent(aObj["content"].toString());
    answerEditDialog->lineEdit_selectAll();
    answerEditDialog->setInjectBId(questionSql->get_value("constructs",aId,"inject").toInt());//初始化要显示的注入绑定8/11

    //exec
    if(answerEditDialog->exec())
    {
        //return
        QString answerType = answerEditDialog->getRetType();
        QString answerContent = answerEditDialog->getRetContent();
        QString answerGoodTime = answerEditDialog->getRetGoodTime().toString("mm'm':ss's'");
        QString answerDisplay = "";
        int answerAId = answerEditDialog->getRetAId();
        int answerPool = answerEditDialog->getRetPool();
        int injectBId = answerEditDialog->getRetInjectBId();

        if(aId != answerAId)
        {
            questionSql->inc_construct_bind_count(answerAId);
            questionSql->del_construct(aId);
        }


        if(answerType == "manual(image)")
        {
            item->setIcon(1,QIcon{answerContent});
            item->setText(1,"");
        }else if(answerType == "auto(typst)") //添加typst数学公式显示功能 25/10/11
        {
            QString imgPath = QString("./data/%1/%2.png").arg(qId).arg(aObj["id"].toInt());
            QIcon icon{imgPath};
            item->setIcon(1,icon);
            item->setText(1,"");
        }else{
            item->setText(1,answerContent);
        }
        questionSql->set_value("constructs",aId,"goodTime",answerGoodTime);
        questionSql->update_question_state(qId);

        item->setText(2,QString::number(answerPool));
        //保存
        aObj["content"] = answerContent;
        aObj["id"] = answerAId;
        aObj["type"] = answerType;
        aObj["pool"] = answerPool;
        aObj["display"] = answerDisplay;
        array[row] = aObj;
        questionSql->write_answerJSON(qId,array);
        questionSql->set_value("constructs",aId,"inject",injectBId);//保存注入绑定8/11

        questionSql->set_value("constructs",aId,"goodTime",answerGoodTime);
    }
}

void MainWindow::on_answerTreeWidget_itemActivated(QTreeWidgetItem *item, int column)
{
    QString condString = "";
    if(item != nullptr)
    {
        //int qId = ui->questionTableView->currentIndex().siblingAtColumn(0).data().toInt();
        int qId = currentQId;
        int row = item->text(0).toInt() - 1;
        QJsonArray answerJson = questionSql->read_answerJSON(qId);
        int aId = answerJson[row].toObject()["id"].toInt();
        condString = QString("id = %1").arg(aId);
    }
    answerTableModel->setFilter(condString);
    answerTableModel->select();
    ui->answerTableView->selectRow(0);
}

void MainWindow::on_answerTreeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    on_answerTreeWidget_itemClicked(current,0);
}

void MainWindow::on_answerTreeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    if(item != nullptr)
    {
        int aId = ui->answerTableView->currentIndex().siblingAtColumn(0).data().toInt();
        //int qId = ui->questionTableView->currentIndex().siblingAtColumn(0).data().toInt();
        int qId = currentQId;

        int row = item->text(0).toInt() - 1;
        QJsonArray answerJson = questionSql->read_answerJSON(qId);
        if(aId != answerJson[row].toObject()["id"].toInt())
            on_answerTreeWidget_itemActivated(item,column);
    }
}

void MainWindow::on_answerTreeWidget_itemChanged(QTreeWidgetItem *item, int column)
{
    if(item != nullptr)
    {
        int aId = ui->answerTableView->currentIndex().siblingAtColumn(0).data().toInt();
        //int qId = ui->questionTableView->currentIndex().siblingAtColumn(0).data().toInt();
        int qId = currentQId;

        int row = item->text(0).toInt() - 1;
        QJsonArray answerJson = questionSql->read_answerJSON(qId);
        if(aId != answerJson[row].toObject()["id"].toInt())
            on_answerTreeWidget_itemActivated(item,column);
    }
}

void MainWindow::answerItemRowsMoved(int startIndex,int endIndex)
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;

    //int qId = index.siblingAtColumn(0).data().toInt();
    int qId = currentQId;

    QJsonArray array = questionSql->read_answerJSON(qId);
    QJsonObject obj = array[startIndex].toObject();
    if(endIndex >= array.count()) endIndex--;
    array.removeAt(startIndex);
    array.insert(endIndex,obj);
    questionSql->write_answerJSON(qId,array);
    load_answer(qId);
    QTreeWidgetItem *item = ui->answerTreeWidget->findItems(QString::number(endIndex + 1),Qt::MatchExactly,0).first();
    ui->answerTreeWidget->setCurrentItem(item);
}

void MainWindow::on_answerAddButton_clicked() //新建答案
{
    QModelIndex qIndex = ui->questionTableView->currentIndex();
    if(!qIndex.isValid())
        return;

    //int qId = qIndex.siblingAtColumn(0).data().toInt();
    int qId = currentQId;

    int row = ui->answerTreeWidget->currentIndex().row() + 1;
    QTreeWidgetItem *newItem = new QTreeWidgetItem{{QString::number(row + 1),"请输入答案"}};
    ui->answerTreeWidget->insertTopLevelItem(row,newItem);
    int id = questionSql->get_max_id("constructs") + 1;
    questionSql->add_construct(id);
    questionSql->inc_construct_bind_count(id);
    questionSql->set_value<QString>("constructs",id,"goodTime","00m:00s");

    //保存
    QJsonArray array = questionSql->read_answerJSON(qId);
    QJsonObject aObj;
    aObj["content"] = "请输入答案";
    aObj["id"] = id;
    aObj["type"] = "auto";
    aObj["pool"] = 0;
    aObj["display"] = "";
    array.insert(row,aObj);
    questionSql->write_answerJSON(qId,array);
    questionSql->update_question_state(qId);
    load_answer(qId);
    newItem = ui->answerTreeWidget->findItems(QString::number(row + 1),Qt::MatchExactly,0).first();
    ui->answerTreeWidget->setCurrentItem(newItem);
    //on_answerTreeWidget_itemActivated(newItem)

    //修改
    on_answerTreeWidget_itemDoubleClicked(newItem,0);
}

void MainWindow::on_answerDelButton_clicked() //删除答案
{
    QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    //int qId = index.siblingAtColumn(0).data().toInt();
    int qId = currentQId;

    QTreeWidgetItem *item = ui->answerTreeWidget->currentItem();
    if(item == NULL)
        return;

    int aId = ui->answerTableView->currentIndex().siblingAtColumn(0).data().toInt();
    int row = item->text(0).toInt() - 1;
    questionSql->del_construct(aId);

    QJsonArray array = questionSql->read_answerJSON(qId);
    array.removeAt(row);
    questionSql->write_answerJSON(qId,array);
    questionSql->update_question_state(qId);

    load_answer(qId);
    item = nullptr;
    if(row >= ui->answerTreeWidget->topLevelItemCount())
        row--;
    auto items = ui->answerTreeWidget->findItems(QString::number(row + 1),Qt::MatchExactly,0);
    if(!items.empty())
        item = items.first();
    ui->answerTreeWidget->setCurrentItem(item);
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
        //int qId = ui->questionTableView->currentIndex().siblingAtColumn(0).data().toInt();
        int qId = currentQId;

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
    setCurrentQId(index.siblingAtColumn(0).data().toInt());

    //load question.html
    QString questionHTML = questionSql->read_questionHTML(currentQId);
    is_questionTextEdit_editable = false;
    ui->questionTextEdit->setHtml(questionHTML);
    is_questionTextEdit_editable = true;
    ui->questionTableView->setCurrentIndex(index);
    load_answer(currentQId);
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
}

void MainWindow::on_questionDelButton_clicked()
{
    int ret = QMessageBox::warning(this, "删除确认",
                                   "即将删除该题目及其文件夹中所有内容\n"
                                   "确定要删除吗?",
                                   QMessageBox::Yes | QMessageBox::No);
    if(ret == QMessageBox::No)
        return;
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
    int qId = currentQId;
    QString questionHTML,questionMd;
    questionHTML = ui->questionTextEdit->toHtml();
    questionSql->write_questionHTML(qId,questionHTML);

    questionMd = ui->questionTextEdit->toMarkdown();
    questionSql->write_questionMd(qId,questionMd);

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
    update_count_categoryTreeView();  //questions学习后不会自动重置列表展开 9/1
    //reload_categoryTreeView();
    if(!index.isValid())
        return;
    //ui->categoryTreeView->setCurrentIndex(categoryItemLists[categoryId][0]->index());

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
        //int qId = ui->questionTableView->currentIndex().siblingAtColumn(0).data().toInt();
        int qId = currentQId;

        QString dirPath = QString("%1/data/%2/").arg(QDir::currentPath()).arg(qId);
        QString filePath = QFileDialog::getOpenFileName(this,
                                                        "打开图片", dirPath, tr("Image Files (*.jpg *.png *.gif)"));
        if(filePath.isNull() || filePath.isEmpty() || filePath == "")
            return;
        QDir dir("./");
        QTextCursor cursor = ui->questionTextEdit->textCursor();
        QTextImageFormat imageFormat;
        imageFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle); //居中对齐插入图片25/10/2
        imageFormat.setName(dir.relativeFilePath(filePath));
        cursor.insertImage(imageFormat);
    }
}

void MainWindow::on_htmlTableAddButton_clicked()
{
    htmlTableAddDialog->setRetRow(0);
    htmlTableAddDialog->setRetColumn(0);
    htmlTableAddDialog->exec();
    QTextTableFormat tf;
    tf.setBorder(1);
    tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
    //tf.setBorderBrush(QBrush(Qt::black))    ;
    tf.setCellPadding(5);
    tf.setBorderCollapse(false);
    int row = htmlTableAddDialog->getRetRow();
    int column = htmlTableAddDialog->getRetColumn();
    ui->questionTextEdit->textCursor().insertTable(row,column,tf);
}

int get_cursor_number(QTextCursor *cursor) //获取当前cursor所在填空的编号9/8
{
    int num = -1;
    ToolFunctions::select_current_underline_text(cursor);
    QString targetText = cursor->selectedText();

    QRegularExpression regex("^\\s{3}[0-9]+\\s{3}$");
    QRegularExpressionMatch match;
    match = regex.match(targetText);
    if(match.hasMatch())
    {
        regex.setPattern("[0-9]+");
        match = regex.match(targetText);
        num = match.capturedTexts()[0].toInt();
    }else
    {
        regex.setPattern("^\\s{3}\\s{3}$");
        match = regex.match(targetText);
        if(match.hasMatch())
            num = 0;
    }
    return num;
}

int MainWindow::autoNumberNext(QTextCursor &cursor) //自动编号下一填空9/29
{
    int number = get_cursor_number(&cursor);
    bool flg = true;
    if(number != -1)//找到下一个，第一个下划线数字文本，并修改为number+1
    {
        flg = false;
        flg = !cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);
        flg = !cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);
        while(!cursor.charFormat().fontUnderline())
        {
            if(!cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1))
            {
                flg = true;
                break;
            }
        }
        if(!flg)
        {
            int nextNumber = get_cursor_number(&cursor);
            if(nextNumber != -1)
            {
                QTextCharFormat format = cursor.charFormat();
                format.setFontUnderline(true);
                cursor.removeSelectedText();
                cursor.setCharFormat(format);
                cursor.insertText("   "+QString::number(number+1)+"   ");
                ui->questionTextEdit->setTextCursor(cursor);
            }
        }
    }
    if(flg)
        return -1;
    else
        return number + 1;
}

void MainWindow::on_autoNumberButton_clicked() //自动编号下一填空9/9
{
    QTextCursor cursor = ui->questionTextEdit->textCursor();
    autoNumberNext(cursor);
}

void MainWindow::on_autoNumberAllButton_clicked() //自动编号后续所有填空9/29
{
    QTextCursor cursor = ui->questionTextEdit->textCursor();
    while(autoNumberNext(cursor) != -1)
    {
        cursor = ui->questionTextEdit->textCursor();
        autoNumberNext(cursor);
    }
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
    ui->categoryTreeView->expand(categoryItemLists[parentId][0]->index());
}

void MainWindow::on_categoryDelButton_clicked()
{
    int ret = QMessageBox::warning(this, "删除确认",
                                   "即将删除该分类及其下所有子分类\n"
                                   "确定要删除吗?",
                                   QMessageBox::Yes | QMessageBox::No);
    if(ret == QMessageBox::No)
        return;
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
    if(currentDate != QDate::currentDate())
    {
        update_count_categoryTreeView();
        currentDate = QDate::currentDate();
    }
    int categoryId = index.siblingAtColumn(1).data().toInt();
    QString condString = questionSql->get_category_condString(categoryId);
    questionTableModel->setFilter(condString);
    questionTableModel->select();
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
    setTimeDialog->initTime(goodTime);
    setTimeDialog->exec();
    QTime time = setTimeDialog->getTime();
    if(!time.isValid())
        return;

    for(auto index : indexes)
    {
        id = index.siblingAtColumn(0).data().toInt();
        QJsonArray answerArray = questionSql->read_answerJSON(id);
        time = QTime::fromMSecsSinceStartOfDay(time.msecsSinceStartOfDay()/answerArray.count()/1000*1000);
        for(auto a : answerArray)
        {
            int aId = a.toObject().value("id").toInt();
            questionSql->set_value("constructs",aId,"goodTime",time.toString("mm'm':ss's'"));
        }
        time =  QTime::fromMSecsSinceStartOfDay(time.msecsSinceStartOfDay()*answerArray.count());
        questionSql->set_value("questions",id,"goodTime",time.toString("mm'm':ss's'"));
    }

    questionTableModel->select();
    QModelIndex newIndex = *questionTableModel->match(questionTableModel->index(0,0),Qt::DisplayRole,firstId).begin();
    ui->questionTableView->setCurrentIndex(newIndex);
    on_questionTableView_activated(newIndex);
}

//自动绑定上一答案为注入，一般适用于文章背诵场景8/11
void MainWindow::on_questionAutoBindInjectButton_clicked()
{
    QJsonArray answerArray = questionSql->read_answerJSON(currentQId);
    int lastCId,nowCId;
    for(int i = 1;i < answerArray.count();i++)
    {
        lastCId = answerArray[i-1].toObject()["id"].toInt();
        nowCId = answerArray[i].toObject()["id"].toInt();
        questionSql->set_value("constructs",nowCId,"inject",lastCId);
    }
}

//按每汉字0.8s自动设置良好时间8/11
void MainWindow::on_questionAutoGoodTimeButton_clicked()
{
    QJsonArray answerArray = questionSql->read_answerJSON(currentQId);
    QRegularExpression regex("[a-z0-9]");
    for(int i = 0;i < answerArray.count();i++)
    {
        int cId = answerArray[i].toObject()["id"].toInt();
        QString answerContent = answerArray[i].toObject()["content"].toString();
        double goodTimeSec = 3.5; //添加3.5s前置时间一般用于读题
        QTime answerGoodTime;
        goodTimeSec += 0.6*(answerContent.count(regex));
        goodTimeSec += 0.8*(answerContent.count() - answerContent.count(regex));
        answerGoodTime = QTime::fromMSecsSinceStartOfDay(qCeil(goodTimeSec)*1000); //向上取整8/14
        questionSql->set_value("constructs",cId,"goodTime",answerGoodTime.toString("mm'm':ss's'"));
    }
    questionSql->update_question_state(currentQId);
}

//重置题目学习数据8/11
void MainWindow::on_questionResetButton_clicked()
{
    QJsonArray answerArray = questionSql->read_answerJSON(currentQId);
    for(int i = 0;i < answerArray.count();i++)
    {
        int cId = answerArray[i].toObject()["id"].toInt();
        questionSql->resetFSRSData(cId);
    }
}

//切换选定部分下划线(快捷键F6) 9/28
void MainWindow::on_underlineToggleButton_clicked()
{
    QTextCursor cursor = ui->questionTextEdit->textCursor();
    int originalPosition = cursor.position();

    if(!cursor.hasSelection()) //若无选择
        ToolFunctions::select_current_underline_text(&cursor);

    if (cursor.hasSelection()) //若有选择
    {
        QTextCharFormat format;
        if(cursor.position() < cursor.selectionEnd())
        {
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            format = cursor.charFormat();
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
        }else
        {
            format = cursor.charFormat();
        }
        format.setFontUnderline(!format.underlineStyle());
        cursor.mergeCharFormat(format);
    }
}

//添加描述条目 25/9/29
void MainWindow::on_descAddButton_clicked()
{
    descAddDialog->exec();
}

void MainWindow::on_questionSearchButton_clicked()
{
    QString condString = QString("questions.name LIKE '\%%1\%'").arg(ui->lineEdit->text());
    questionTableModel->setFilter(condString);
    questionTableModel->select();
}

//添加id跳转 25/9/29
void MainWindow::on_idLineEdit_returnPressed()
{
    int qId = ui->idLineEdit->text().toInt();
    QSqlQuery query(QSqlDatabase::database("connection1"));
    QString queryString = QString("SELECT categoryId FROM questions WHERE id = %1").arg(qId);
    query.exec(queryString);
    if(query.next())
    {
        int cId  = query.value(0).toInt();
        ui->categoryTreeView->setCurrentIndex(categoryItemLists[cId][0]->index());
        on_categoryTreeView_clicked(categoryItemLists[cId][0]->index());
        select_question_by_id(qId);
    }
}

//添加题目搜索 25/9/29
void MainWindow::on_lineEdit_returnPressed()
{
    on_questionSearchButton_clicked();
}

//添加题目历史跳转功能 25/9/29
void MainWindow::setCurrentQId(int qId)
{
    currentQId = qId;
    ui->idLineEdit->setText(QString::number(qId));
    if(qIdHistoryPos == 0 ||
        qId != qIdHistoryQueue[qIdHistoryPos - 1])
    {
        qIdHistoryQueue.insert(qIdHistoryPos,qId);
        qIdHistoryPos = qIdHistoryPos >= 100? 100 : qIdHistoryPos + 1;
        if(qIdHistoryQueue.size() > 100)
            qIdHistoryQueue.dequeue();
    }
}

//添加题目历史跳转功能 向前 25/9/29
void MainWindow::on_prevQButton_clicked()
{
    if(qIdHistoryPos == 0)
        return;
    qIdHistoryPos = qIdHistoryPos <= 1? 1 : qIdHistoryPos - 1;
    QString prevQId = QString::number(qIdHistoryQueue[qIdHistoryPos-1]);
    ui->idLineEdit->setText(prevQId);
    on_idLineEdit_returnPressed();
}

//添加题目历史跳转功能 向后 25/9/29
void MainWindow::on_nextQButton_clicked()
{
    if(qIdHistoryPos == 0)
        return;
    qIdHistoryPos = qIdHistoryPos >= qIdHistoryQueue.size()? qIdHistoryQueue.size() : qIdHistoryPos + 1;
    QString nextQId = QString::number(qIdHistoryQueue[qIdHistoryPos-1]);
    ui->idLineEdit->setText(nextQId);
    on_idLineEdit_returnPressed();
}
