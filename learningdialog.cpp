#include "learningdialog.h"
#include "ui_learningdialog.h"
#include "toolfunctions.h"
#include "fsrs.h"

LearningDialog::LearningDialog(QuestionSql *newQuestionSql,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LearningDialog)
{
    ui->setupUi(this);

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
        QLabel* answerTimeLabel = qobject_cast<QLabel*>(layout->itemAtPosition(row,3)->widget());
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
    tableModel->setHeaderData(2, Qt::Horizontal, "状态");
    tableModel->setHeaderData(3, Qt::Horizontal, "名称");
    tableModel->setHeaderData(4, Qt::Horizontal, "平均评分");
    tableModel->setHeaderData(6, Qt::Horizontal, "良好用时");
    tableModel->setHeaderData(5, Qt::Horizontal, "平均用时");
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
        ui->tableView->sortByColumn(21,Qt::AscendingOrder);
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
        ui->tableView->sortByColumn(21,Qt::AscendingOrder);
    }
    ui->tableView->clearSelection();
    clear_question_display();
}
void LearningDialog::answer_lineEdit_textChanged(const QString &arg1) //自动扩充输入框
{
    QLineEdit* senderLineEdit = (QLineEdit*)sender();
    int pixelWide = senderLineEdit->fontMetrics().horizontalAdvance(arg1) + 15;
    senderLineEdit->setFixedWidth(pixelWide);
    //ui->lineEdit->setMaximumWidth(pixelWide);
}
void LearningDialog::on_tableView_clicked(const QModelIndex &index)
{
    int id = index.siblingAtColumn(0).data().toInt();
    set_question(id);
}

//核心功能
void LearningDialog::set_question(int id)
{
    preSubmited = false;
    submited = false;
    currentId = id;
    ui->idLabel->setText(QString("id:%1").arg(currentId));

    QString html = questionSql->read_questionHTML(id);
    QJsonArray array = questionSql->read_answerJSON(id);

    clear_question_display();
    ui->textBrowser->setHtml(html);

    //set answer
    QGridLayout *layout = (QGridLayout *)ui->groupBox->layout();

    //add items
    rowCount = array.count();
    for(int i = 0;i < array.count();i++)
    {
        QLineEdit* newLineEdit = new QLineEdit(this);
        QLabel* newAnswerLabel = new QLabel(this); //StatusTip:id ToolTip:type
        QCheckBox* newCheackBox = new QCheckBox(this);
        QLabel* newCheckLabel = new QLabel(this);
        QLabel* newTimeLabel = new QLabel(this);
        QLabel* newGoodTimeLabel = new QLabel(this);

        int pixelWide = 10;

        QJsonObject obj = array[i].toObject();
        newAnswerLabel->setStatusTip(QString::number(obj["id"].toInt()));
        newAnswerLabel->setToolTip(obj["type"].toString());
        newLineEdit->setToolTip(QString::number(i)); //row

        QString timeString = ToolFunctions::ms2msz(questionSql->get_value("answers",obj["id"].toInt(),"goodTime").toString());
        newTimeLabel ->setText("00:00.00");
        newGoodTimeLabel->setText(timeString);

        if(obj["type"].toString() != "manual(image)")
        {
            newAnswerLabel->setText(obj["content"].toString());
            pixelWide = fm->horizontalAdvance(array[i].toObject().value("content").toString());
        }else
        {
            newAnswerLabel->setPixmap(QPixmap{obj["content"].toString()});
        }
        connect(newLineEdit,&QLineEdit::textChanged,this,&LearningDialog::answer_lineEdit_textChanged);

        newLineEdit->setFixedWidth(pixelWide + 15);
        newCheckLabel->setText("错误");
        newCheackBox->setChecked(false);

        QString nextDateString = questionSql->get_value("answers",obj["id"].toInt(),"nextDate").toString();
        QDate nextDate = QDate::fromString(nextDateString,"yyyy/MM/dd");
        if(onlyToLearn && nextDate > QDate::currentDate())
        {
            layout->addWidget(newAnswerLabel,i,0);
        }else
        {
            layout->addWidget(newLineEdit,i,0);
            layout->addWidget(newAnswerLabel,i,1);
            layout->addWidget(newCheackBox,i,2);
            layout->addWidget(newTimeLabel,i,3);
            layout->addWidget(newGoodTimeLabel,i,4);

            newAnswerLabel->hide();
            newCheackBox->hide();
        }
    }

    //添加整个问题的作答时间、回答情况
    timeLabel = new QLabel(this);
    QLabel* newGoodTimeLabel = new QLabel(this);
    QLabel* newCheckLabel = new QLabel(this);
    timeLabel->setText("00:00.00");
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

    layout->itemAt(0)->widget()->setFocus();

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
        if(layout->itemAtPosition(row,1) == nullptr)
        {
            continue;
        }
        QLineEdit *lineEdit = (QLineEdit *)layout->itemAtPosition(row,0)->widget();
        QLabel *answerLabel = (QLabel *)layout->itemAtPosition(row,1)->widget();
        QCheckBox *checkBox = (QCheckBox *)layout->itemAtPosition(row,2)->widget();

        if(answerLabel->toolTip() == "auto")
        {
            if(lineEdit->text() == answerLabel->text())
                checkBox->setChecked(true);
            else
                checkBox->setChecked(false);
            checkBox->setEnabled(false);
        }

        lineEdit->setEnabled(false);
        answerLabel->show();
        checkBox->show();
    }
}
void LearningDialog::submit()
{
    submited = true;

    QGridLayout *layout = (QGridLayout *)ui->groupBox->layout();
    int row = 0;
    bool wrong = false;

    //检查每个答案作答情况
    for(;row < rowCount;row++)
    {
        if(layout->itemAtPosition(row,1) == nullptr)
            continue;
        QLabel *answerLabel = qobject_cast<QLabel*>(layout->itemAtPosition(row,1)->widget());
        QCheckBox *checkBox = (QCheckBox *)layout->itemAtPosition(row,2)->widget();
        QString answerTimeString = qobject_cast<QLabel*>(layout->itemAtPosition(row,3)->widget())->text();
        QTime answerTime = ToolFunctions::msz2QTime(answerTimeString);

        if(checkBox->isChecked() == false)
        {
            wrong = true;
            answerTime = QTime::fromMSecsSinceStartOfDay(0);
        }
        int aId = answerLabel->statusTip().toInt();

        questionSql->update_answer_state(aId,answerTime);
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
            QString state = tableModel->index(oldRow,2).data().toString();
            if(state == "learning")
            {
                int shortTimeCount = 0;
                int row = oldRow + 1;
                while(shortTimeCount < 30 && row < tableModel->rowCount())
                {
                    QTime gTime = QTime().fromString(tableModel->index(oldRow,5).data().toString(),"mm'm':ss's'");
                    shortTimeCount += gTime.msecsSinceStartOfDay()/1000;
                    row++;
                }
                int orderNum1 = tableModel->index(row - 1,21).data().toInt();
                int orderNum2 = 32767;
                if(row < tableModel->rowCount())
                {
                    orderNum2 = tableModel->index(row,21).data().toInt();
                }
                int newOrderNum = (orderNum1 + orderNum2) / 2;
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
        preSubmit();
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

