#include "learningdialog.h"
#include "ui_learningdialog.h"
#include "toolfunctions.h"
#include "core/fsrs.h"
#include <QListView>
#include <QSet>

LearningDialog::LearningDialog(QuestionSql *newQuestionSql,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LearningDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    //init timer
    timer = new QTimer(this);
    timer->setInterval(10);
    connect(timer,&QTimer::timeout,this,&LearningDialog::timerHandler);

    //init total timer
    totalTimer = new QTimer(this);
    totalTimer->setInterval(10);
    connect(totalTimer,&QTimer::timeout,this,&LearningDialog::totalTimerHandler);

    font = QApplication::font();
    fm = new QFontMetrics(font);
    time.setHMS(0,0,0);
    questionSql = newQuestionSql;
    submited = true;
    currentId = -1;

    //init table
    tableModel = new QSqlRelationalTableModel(this,questionSql->getDb());
    tableModel->setTable("questions");
    tableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    tableModel->setRelation(1,QSqlRelation{"categories","id","name"});

    ui->tableView->setModel(tableModel);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    set_tableHeader();

    //set text broswer
    ui->textBrowser->setTabStopDistance(ui->textBrowser->fontMetrics().horizontalAdvance(' ')*4);

    //init isSpeedLearn
    isSpeedLearn = false;

    //init answer grid layout
    QGridLayout *answerBoxLayout = qobject_cast<QGridLayout*>(ui->groupBox->layout());
}

LearningDialog::~LearningDialog()
{
    delete ui;
}

//timer
void LearningDialog::start_timer()
{
    time.setHMS(0,0,0);
    submited = false;
    timer->start();
}
void LearningDialog::stop_timer()
{
    timer->stop();
}
void LearningDialog::timerHandler()
{
    //更新整个问题的作答时间
    time = time.addMSecs(10);
    if(timeLabel)
    {
        timeLabel->setText(time.toString("mm:ss.zzz").chopped(1));
    }

    //更新当前答案的作答时间
    QLineEdit *currentLineEdit = ToolFunctions::get_active_LineEdit();
    if(currentLineEdit != nullptr)
    {
        int row = currentLineEdit->toolTip().toInt();
        QGridLayout* layout = qobject_cast<QGridLayout*>(currentLineEdit->parentWidget()->layout());
        QLabel* answerTimeLabel = qobject_cast<QLabel*>(layout->itemAtPosition(row,4)->widget());
        QTime answerTime = ToolFunctions::msz2QTime(answerTimeLabel->text());
        answerTime = answerTime.addMSecs(10);
        answerTimeLabel->setText(answerTime.toString("mm:ss.zzz").chopped(1));
    }
}
void LearningDialog::totalTimerHandler()
{
    totalTime = totalTime.addMSecs(10);
    ui->totalTimeLabel->setText(totalTime.toString("hh:mm:ss.zzz").chopped(1));
}

//getter
QTime LearningDialog::getTime() const
{
    return time;
}
int LearningDialog::getLastId() const
{
    return lastId;
}
QTime LearningDialog::getTotalTime() const
{
    return totalTime;
}

//setter
void LearningDialog::setIsSpeedLearn(bool newIsSpeedLearn)
{
    isSpeedLearn = newIsSpeedLearn;
}
bool LearningDialog::getOnlyToLearn() const
{
    return onlyToLearn;
}
void LearningDialog::setOnlyToLearn(bool newOnlyToLearn)
{
    onlyToLearn = newOnlyToLearn;
}
void LearningDialog::set_tableHeader()
{
    tableModel->setHeaderData(1, Qt::Horizontal, "类别");
    tableModel->setHeaderData(2, Qt::Horizontal, "名称");
    tableModel->setHeaderData(3, Qt::Horizontal, "状态");
    tableModel->setHeaderData(4, Qt::Horizontal, "平均评分");
    tableModel->setHeaderData(5, Qt::Horizontal, "良好用时");
    tableModel->setHeaderData(6, Qt::Horizontal, "平均用时");
    tableModel->setHeaderData(7, Qt::Horizontal, "最佳用时");
    tableModel->setHeaderData(8, Qt::Horizontal, "到期");
    tableModel->setHeaderData(9, Qt::Horizontal, "最近学习");
}
void LearningDialog::set_items_table(QString filter) //设置表过滤
{
    totalTime = QTime::fromMSecsSinceStartOfDay(0);
    tableModel->setFilter(filter);
    totalCount = 0;
    correctCount = 0;
    if(isSpeedLearn)
        ui->comboBox->setCurrentIndex(1);
    clear_question_display();
    ui->tableView->sortByColumn(0,Qt::AscendingOrder);
    on_comboBox_currentTextChanged(ui->comboBox->currentText());
}

//checked
bool LearningDialog::is_submited()
{
    return submited;
}
void LearningDialog::clear_question_display()
{
    //clear items
    ui->textBrowser->setText("");
    QGridLayout *layout = (QGridLayout *)ui->groupBox->layout();
    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr)
    {
        delete child->widget();
        delete child;
    }
}
void LearningDialog::on_comboBox_currentTextChanged(const QString &arg1) //排序
{
    stop_timer();
    QSqlQuery query(questionSql->getDb());

    if(arg1 == "随机排序")
    {
        int rowCount = tableModel->rowCount();
        for(int i = 0;i < rowCount;i++)
        {
            int randNum = QRandomGenerator::global()->bounded(32768);
            int id = tableModel->index(i,0).data().toInt();
            questionSql->set_value("questions",id,"orderNum",randNum);
        }
        ui->tableView->setSortingEnabled(true);
        ui->tableView->sortByColumn(10,Qt::AscendingOrder);
    }else if(arg1 == "默认排序")
    {
        int rowCount = tableModel->rowCount();
        int gap = rowCount == 0? 0:32767 / rowCount;
        for(int i = 0;i < rowCount;i++)
        {
            int id = tableModel->index(i,0).data().toInt();
            questionSql->set_value("questions",id,"orderNum",i * gap);
        }
        ui->tableView->setSortingEnabled(true);
        ui->tableView->sortByColumn(10,Qt::AscendingOrder);
    }
    ui->tableView->clearSelection();
    clear_question_display();
}
void LearningDialog::answer_lineEdit_textChanged(const QString &arg1) //自动扩充输入框
{
    currentLineEdit = (QLineEdit*)sender();
    int pixelWide = currentLineEdit->fontMetrics().horizontalAdvance(arg1) + 15;
    if(pixelWide > currentLineEdit->statusTip().toInt()) //设置最小宽度为初始宽度8/14
        currentLineEdit->setFixedWidth(pixelWide);
    else
        currentLineEdit->setFixedWidth(currentLineEdit->statusTip().toInt());
}

void LearningDialog::on_tableView_clicked(const QModelIndex &index)
{
    int id = index.siblingAtColumn(0).data().toInt();
    set_question(id);
}

void LearningDialog::poolComboBox_currentIndexChanged(const int &index)
{
    QComboBox* senderComboBox = qobject_cast<QComboBox*>(sender());

    //自动调整乱序选框大小2025/3/9
    if(!senderComboBox->itemIcon(index).isNull())
    {
        QSize sz = senderComboBox->itemIcon(index).actualSize(QSize{1000,1000});
        sz.setWidth(35 + sz.width());
        sz.setHeight(3 + sz.height());
        senderComboBox->setFixedSize(sz);
        senderComboBox->setIconSize(senderComboBox->itemIcon(index).actualSize(QSize{1000,1000}));
        senderComboBox->view()->setIconSize(QSize{1000,1000});
    }else
    {
        int w = 35 + senderComboBox->fontMetrics().horizontalAdvance(senderComboBox->itemText(index));
        senderComboBox->setFixedSize(w,28);
    }

    //不和其他选框内容重复
    if(!senderComboBox->itemData(index).isValid())
        return;

    QGridLayout *layout = (QGridLayout *)ui->groupBox->layout();
    int ComboxRow = senderComboBox->itemData(index).toInt();
    QJsonObject ComboxObj = currentArray[ComboxRow].toObject();
    int pool = ComboxObj["pool"].toInt();
    int currentRow;
    QMap<int,int> used_row;
    for(int i = 0;i < senderComboBox->count();i++) //统计该乱序池中所有的答案行号
    {
        int answerRow = senderComboBox->itemData(i).toInt();
        used_row[answerRow] = -1;
    }

    //统计所有已使用的答案原行号到实际行号的映射
    for(int i = 0;i < rowCount;i++)
    {
        if(layout->itemAtPosition(i,2) == nullptr)
            continue;
        QComboBox* comBox = qobject_cast<QComboBox*>(layout->itemAtPosition(i,2)->widget());
        if(comBox == nullptr)
            continue;
        if(currentArray[i].toObject()["pool"].toInt() != pool)
            continue;
        if(comBox == senderComboBox)
            currentRow = i;

        int comBoxSelectedRow = comBox->itemData(comBox->currentIndex()).toInt();
        used_row[comBoxSelectedRow] = i;
    }
    used_row[ComboxRow] = currentRow;

    //如果和已有冲突，则换成其他可用行号
    for(int i = 0;i < rowCount;i++)
    {
        if(layout->itemAtPosition(i,2) == nullptr)
            continue;
        QComboBox* comBox = qobject_cast<QComboBox*>(layout->itemAtPosition(i,2)->widget());
        if(comBox == nullptr)
            continue;
        if(currentArray[i].toObject()["pool"].toInt() != pool)
            continue;
        if(comBox == senderComboBox)
            continue;

        int comBoxSelectedRow = comBox->itemData(comBox->currentIndex()).toInt();
        if(used_row[comBoxSelectedRow] != i) //如果记录的行不映射到当前行，说明有冲突
        {
            for(auto k : used_row.keys())
            {
                if(used_row[k] == -1)
                {
                    comBoxSelectedRow = k;
                    used_row[comBoxSelectedRow] = i;
                    comBox->setCurrentIndex(comBox->findData(k));
                    break;
                }
            }
        }
    }
}

//核心功能
void LearningDialog::set_question(int id)
{
    preSubmited = false;
    submited = false;
    currentId = id;
    currentLineEdit = nullptr; //最初currentLineEdit设为nullptr 8/14
    ui->idLabel->setText(QString("id:%1").arg(currentId));

    QString html = questionSql->read_questionHTML(id);
    currentArray = questionSql->read_answerJSON(id);
    QJsonArray &array = currentArray;
    QSet<int> toLearnPoolSet; //记录需要复习的乱序池8/10
    QSet<int> toLearnInjectSet; //记录需要显示的注入答案8/10

    clear_question_display();
    ui->textBrowser->setHtml(html);

    //set answer
    QGridLayout *layout = (QGridLayout *)ui->groupBox->layout();

    //add items
    rowCount = array.count();
    for(int i = 0;i < array.count();i++)
    {
        QLabel* newLineNumberLabel = new QLabel(this);
        QLineEdit* newLineEdit = new QLineEdit(this); //StatusTip:FixedWidth
        QLabel* newAnswerLabel = new QLabel(this); //StatusTip:id ToolTip:type
        QCheckBox* newCheackBox = new QCheckBox(this);
        QLabel* newCheckLabel = new QLabel(this);
        QLabel* newTimeLabel = new QLabel(this);
        QLabel* newGoodTimeLabel = new QLabel(this);

        int pixelWide = 10;

        newLineNumberLabel->setText(QString::number(i+1));

        QJsonObject obj = array[i].toObject();
        newAnswerLabel->setStatusTip(QString::number(obj["id"].toInt()));
        newAnswerLabel->setToolTip(obj["type"].toString());
        newLineEdit->setToolTip(QString::number(i)); //row


        QString timeString = ToolFunctions::ms2msz(questionSql->get_value("constructs",obj["id"].toInt(),"goodTime").toString());
        newTimeLabel ->setText("00:00.00");
        newGoodTimeLabel->setText(timeString);

        if(obj["type"].toString() == "auto")
        {
            newAnswerLabel->setText(obj["content"].toString());
            pixelWide = fm->horizontalAdvance(array[i].toObject().value("content").toString());
        }
        else if(obj["type"].toString() == "manual")
        {
            newLineEdit->setStyleSheet("QLineEdit {border: 1px solid blue;}");
            newAnswerLabel->setText(obj["content"].toString());
            pixelWide = fm->horizontalAdvance(array[i].toObject().value("content").toString());
        }else if(obj["type"].toString() == "manual(image)")
        {
            newLineEdit->setStyleSheet("QLineEdit {border: 1px solid red;}");
            newAnswerLabel->setPixmap(QPixmap{obj["content"].toString()});
        }
        connect(newLineEdit,&QLineEdit::textChanged,this,&LearningDialog::answer_lineEdit_textChanged);

        newLineEdit->setStatusTip(QString::number(pixelWide + 15)); //记录LineEdit初始长度到StatusTip 8/14
        newLineEdit->setFixedWidth(pixelWide + 15);
        newCheckLabel->setText("错误");
        newCheackBox->setChecked(false);

        QString nextDateString = questionSql->get_value("constructs",obj["id"].toInt(),"nextDate").toString();
        QDate nextDate = QDate::fromString(nextDateString,"yyyy/MM/dd");


        layout->addWidget(newLineNumberLabel,i,0);
        if(onlyToLearn && nextDate > QDate::currentDate())
        {//隐藏不需复习的答案25/8/2
            layout->addWidget(newAnswerLabel,i,1);
            newLineNumberLabel->hide();
            newAnswerLabel->hide();
        }else
        {
            layout->addWidget(newLineEdit,i,1);
            layout->addWidget(newAnswerLabel,i,2);
            layout->addWidget(newCheackBox,i,3);
            layout->addWidget(newTimeLabel,i,4);
            layout->addWidget(newGoodTimeLabel,i,5);

            newAnswerLabel->hide();
            newCheackBox->hide();

            //记录需要复习的乱序池8/10
            int currentPool = obj["pool"].toInt();
            if(currentPool != 0)
                toLearnPoolSet.insert(currentPool);

            //记录需要显示的注入8/11
            int inject = questionSql->get_value("constructs",obj["id"].toInt(),"inject").toInt();
            toLearnInjectSet.insert(inject);

            //初始化currentLineEdit 8/14
            if(currentLineEdit == nullptr) currentLineEdit = newLineEdit;
        }
    }


    for(int row = 0;row < array.count();row++)
    {
        if(layout->itemAtPosition(row,2) == nullptr)
        {
            if(toLearnPoolSet.contains(array[row].toObject()["pool"].toInt()) || //显示被隐藏的需要复习的乱序池中，不需要复习的答案8/10
                toLearnInjectSet.contains(array[row].toObject()["id"].toInt()))  //显示不许复习但需要显示的作为注入的答案8/11
            {
                QLabel* lineNumberLabel = qobject_cast<QLabel*>(layout->itemAtPosition(row,0)->widget());
                QLabel* answerLabel = qobject_cast<QLabel*>(layout->itemAtPosition(row,1)->widget());
                lineNumberLabel->show();
                answerLabel->show();
            }
            continue;
        }

        //乱序池
        QJsonObject obj = array[row].toObject();

        if(obj["pool"].toInt() != 0)
        {
            QComboBox* newAnswerComboBox = new QComboBox(this);
            newAnswerComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            newAnswerComboBox->setIconSize(QSize{1000,1000});
            connect(newAnswerComboBox,&QComboBox::currentIndexChanged,this,&LearningDialog::poolComboBox_currentIndexChanged);

            for(int row = 0;row < array.count();row++) //遍历添加乱序选框选项
            {
                if(layout->itemAtPosition(row,2) == nullptr)
                    continue;
                QJsonObject pool_obj = array[row].toObject();
                if(pool_obj["pool"].toInt() == obj["pool"].toInt())
                {
                    if(pool_obj["type"].toString() == "manual(image)")
                    {
                        newAnswerComboBox->addItem(QIcon{pool_obj["content"].toString()},"",row);
                    }else
                    {
                        newAnswerComboBox->addItem(pool_obj["content"].toString(),row);
                    }
                }
            }
            newAnswerComboBox->setCurrentIndex(newAnswerComboBox->findData(row));
            layout->removeItem(layout->itemAtPosition(row,2));
            layout->addWidget(newAnswerComboBox,row,2);
            newAnswerComboBox->hide();
        }
    }

    //添加整个问题的作答时间、回答情况
    timeLabel = new QLabel(this);
    QLabel* newGoodTimeLabel = new QLabel(this);
    QLabel* newCheckLabel = new QLabel(this);
    timeLabel->setText("00:00.00");
    timeLabel->setFixedWidth(80);
    QString timeString = ToolFunctions::ms2msz(questionSql->get_value("questions",id,"goodTime").toString());
    newGoodTimeLabel->setText(timeString);
    newCheckLabel->setText("良好");

    layout->addWidget(timeLabel,array.count(),0);
    layout->addWidget(newGoodTimeLabel,array.count(),1);
    layout->addWidget(newCheckLabel,array.count(),2);

    newCheckLabel->hide();

    //add spacer
    QSpacerItem* verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    layout->addItem(verticalSpacer,array.count()+1,0,1,5);

    //layout->itemAt(0)->widget()->setFocus();

    if(!totalTimer->isActive()) totalTimer->start();
    start_timer();
}
void LearningDialog::preSubmit()
{
    preSubmited = true;
    QGridLayout *layout = (QGridLayout *)ui->groupBox->layout();
    int row = 0;

    for(;row < rowCount;row++)
    {
        if(layout->itemAtPosition(row,2) == nullptr)
        {
            //显示被隐藏的答案25/8/2
            QLabel* lineNumberLabel = qobject_cast<QLabel*>(layout->itemAtPosition(row,0)->widget());
            QLabel* answerLabel = qobject_cast<QLabel*>(layout->itemAtPosition(row,1)->widget());
            lineNumberLabel->show();
            answerLabel->show();
            continue;
        }

        QJsonArray array = questionSql->read_answerJSON(currentId);
        QLineEdit *lineEdit = (QLineEdit *)layout->itemAtPosition(row,1)->widget();
        QCheckBox *checkBox = (QCheckBox *)layout->itemAtPosition(row,3)->widget();
        lineEdit->setEnabled(false);
        checkBox->show();

        //非乱序池答案
        if(array[row].toObject()["pool"].toInt() == 0)
        {
            QLabel *answerLabel = (QLabel *)layout->itemAtPosition(row,2)->widget();
            if(answerLabel->toolTip() == "auto" ||
                answerLabel->toolTip() == "manual") //如果是非图片手动检查，默认也检查一下是否匹配25/8/21
            {
                if(lineEdit->text() == answerLabel->text())
                    checkBox->setChecked(true);
                else
                    checkBox->setChecked(false);
                checkBox->setEnabled(false);
            }
            if(answerLabel->toolTip() == "manual" && !checkBox->isChecked()) //如果是非图片手动检查，则不匹配时重新启用cheackbox
            {
                checkBox->setEnabled(true);
            }
            answerLabel->show();
        }

        //乱序池答案，自动检查通过的从其他乱序池答案的选框项中移除
        if(array[row].toObject()["pool"].toInt() != 0)
        {
            QComboBox *answerComboBox = (QComboBox *)layout->itemAtPosition(row,2)->widget();

            QJsonObject currentObj = array[row].toObject();

            if(currentObj["type"].toString() == "auto")
            {
                checkBox->setChecked(false);
                for(int i = 0;i < answerComboBox->count();i++) //遍历所有乱序池以检查答案
                {
                    int _row = answerComboBox->itemData(i).toInt();
                    QJsonObject _obj = array[_row].toObject();
                    if(_obj["type"].toString() == "auto" &&
                       lineEdit->text() == _obj["content"].toString())
                    {
                        checkBox->setChecked(true);
                        answerComboBox->setCurrentIndex(i);
                        break;
                    }
                }
                checkBox->setEnabled(false);

                if(checkBox->isChecked())
                {
                    answerComboBox->setEnabled(false);
                    for(int _row = 0;_row < rowCount;_row++) //遍历其他乱序框以删除
                    {
                        if(_row == row) continue;

                        QJsonObject _obj = array[_row].toObject();
                        if(_obj["pool"].toInt() == currentObj["pool"].toInt() &&
                            layout->itemAtPosition(_row,2) != nullptr)
                        {
                            QComboBox *_answerComboBox = (QComboBox *)layout->itemAtPosition(_row,2)->widget();
                            _answerComboBox->removeItem(_answerComboBox->findText(answerComboBox->currentText()));
                        }
                    }
                }
            }

            answerComboBox->show();
        }
    }

    QTimer::singleShot(2, this, [this](){ //延迟2ms后再进行聚焦以避免布局更新未完成8/15
        if(currentLineEdit != nullptr) //提交时聚焦到currentLineEdit 8/14
            ui->scrollArea->ensureWidgetVisible(currentLineEdit);
    });
}
void LearningDialog::submit()
{
    submited = true;

    QGridLayout *layout = (QGridLayout *)ui->groupBox->layout();
    int row = 0;
    bool wrong = false;
    QJsonArray array = questionSql->read_answerJSON(currentId);

    //检查每个答案作答情况
    for(;row < rowCount;row++)
    {
        QJsonObject currentObj = array[row].toObject();
        if(layout->itemAtPosition(row,2) == nullptr)
            continue;
        //QLabel *answerLabel = qobject_cast<QLabel*>(layout->itemAtPosition(row,1)->widget());
        QCheckBox *checkBox = (QCheckBox *)layout->itemAtPosition(row,3)->widget();
        QString answerTimeString = qobject_cast<QLabel*>(layout->itemAtPosition(row,4)->widget())->text();
        QTime answerTime = ToolFunctions::msz2QTime(answerTimeString);

        if(checkBox->isChecked() == false)
        {
            wrong = true;
            answerTime = QTime::fromMSecsSinceStartOfDay(0);
        }
        int aId = currentObj["id"].toInt();
        if(currentObj["pool"] != 0) //如果答案属于乱序池则根据乱序选框确定aId
        {
            QComboBox *answerComboBox = qobject_cast<QComboBox*>(layout->itemAtPosition(row,2)->widget());
            int _row = answerComboBox->itemData(answerComboBox->currentIndex()).toInt();
            aId = array[_row].toObject()["id"].toInt();
        }
        //int aId = answerLabel->statusTip().toInt();

        questionSql->update_construct_state(aId,answerTime);
        checkBox->setEnabled(false);
    }

    //QLabel *timeLabel = (QLabel *)layout->itemAtPosition(row,0)->widget();
    QLabel *goodTimeLabel = (QLabel *)layout->itemAtPosition(row,1)->widget();
    QLabel *checkLabel = (QLabel *)layout->itemAtPosition(row,2)->widget();

    QTime goodTime = QTime::fromString(goodTimeLabel->text(),"mm:ss.zz");
    rating = FSRS::time2rating(time,goodTime);
    if(rating == "hard")        checkLabel->setText("困难");
    else if(rating == "easy")   checkLabel->setText("简单");
    else if(rating == "good")   checkLabel->setText("良好");
    if(wrong)
    {
        rating = "wrong";
        time = QTime::fromMSecsSinceStartOfDay(0);
        checkLabel->setText("出错");
    }

    questionSql->update_question_state(currentId);
    checkLabel->show();
    oldRow = ui->tableView->currentIndex().row();

    //set accuracy label
    totalCount++;
    if(!wrong)
        correctCount++;
    QString acc = QString::number(double(correctCount)/totalCount*100,'f',2);
    QString newAccuracyText = QString("正确率(%1\%):%2/%3").arg(acc).arg(correctCount).arg(totalCount);
    ui->accuracyLabel->setText(newAccuracyText);

    //set new filter
    if(isSpeedLearn)
    {
        if(!wrong)
        {
            QString condString = QString("(%1) AND questions.id != %2").arg(tableModel->filter()).arg(currentId);
            tableModel->setFilter(condString);
        }
    }

    //update old row
    tableModel  ->select();
    if(oldRow >= tableModel->rowCount())
        oldRow = 0;
    ui->tableView->selectRow(oldRow);
}
void LearningDialog::on_pushButton_clicked()
{
    stop_timer();

    if(tableModel->rowCount() == 0)
    {
        totalTimer->stop();
        return;
    }
    if(currentId == -1)
        return;

    QMessageBox msgbox;
    msgbox.setText("1");

    //提交后再点击切换题目、生成随机排序数
    if(submited)
    {
        int newCurrentId = tableModel->index(oldRow,0).data().toInt();

        //更新随机排序数
        if(ui->comboBox->currentText() == "随机排序" && newCurrentId == currentId)
        {
            int randNum = QRandomGenerator::global()->bounded(32768);
            questionSql->set_value("questions",newCurrentId,"orderNum",randNum);
            tableModel->select();
        }else if(ui->comboBox->currentText() == "默认排序" && newCurrentId == currentId)
        {
            QString state = tableModel->index(oldRow,3).data().toString();
            qDebug() << state;
            if(state == "learning" || state == "new")
            {
                int shortTimeCount = 0;
                int row = oldRow + 1;
                while(shortTimeCount < 30 && row < tableModel->rowCount())
                {
                    QTime gTime = QTime().fromString(tableModel->index(oldRow,5).data().toString(),"mm'm':ss's'");
                    shortTimeCount += gTime.msecsSinceStartOfDay()/1000;
                    row++;
                }
                int orderNum1 = tableModel->index(row - 1,QuestionSql::headerColumnMap["orderNum"]).data().toInt();
                int orderNum2 = 32767;
                if(row < tableModel->rowCount())
                {
                    orderNum2 = tableModel->index(row,QuestionSql::headerColumnMap["orderNum"]).data().toInt();
                }
                int newOrderNum = (orderNum1 + orderNum2) / 2;
                qDebug() << orderNum1 << orderNum2 << newOrderNum;
                questionSql->set_value("questions",newCurrentId,"orderNum",newOrderNum);
                tableModel->select();
            }
        }

        newCurrentId = tableModel->index(oldRow,0).data().toInt();

        //切换题目
        int newRow = oldRow;
        if(newCurrentId == currentId)
        {
            newRow = oldRow + 1;
            if(newRow >= tableModel->rowCount())
                newRow = 0;
        }

        ui->tableView->selectRow(newRow);
        set_question(ui->tableView->currentIndex().siblingAtColumn(0).data().toInt());

        submited = false;
        return;
    }

    if(!preSubmited)
    {
        preSubmit();
    }
    else if(!submited)
    {
        preSubmited = false;
        submit();
    }
}
void LearningDialog::on_LearningDialog_finished(int result)
{
    stop_timer();
    totalTimer->stop();
    submited = true;
    lastId = currentId;
    currentId = -1;
    if(tableModel->rowCount() != 0)
    {
        totalTime = QTime::fromString("23:59:59.99");
    }
}

