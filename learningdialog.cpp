#include "learningdialog.h"
#include "ui_learningdialog.h"
#include "toolfunctions.h"
#include "fsrs.h"

LearningDialog::LearningDialog(QuestionSql *newQuestionSql,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LearningDialog)
{
    ui->setupUi(this);
    timer = new QTimer(this);
    timer->setInterval(10);
    connect(timer,&QTimer::timeout,this,&LearningDialog::timerHandler);
    font = new QFont("Microsoft YaHei",9);
    fm = new QFontMetrics(*font);
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

}

LearningDialog::~LearningDialog()
{
    delete ui;
}

void LearningDialog::set_tableHeader()
{
    tableModel->setHeaderData(1, Qt::Horizontal, "类别");
    tableModel->setHeaderData(2, Qt::Horizontal, "状态");
    tableModel->setHeaderData(3, Qt::Horizontal, "平均评分");
    tableModel->setHeaderData(4, Qt::Horizontal, "平均用时");
    tableModel->setHeaderData(5, Qt::Horizontal, "良好用时");
    tableModel->setHeaderData(6, Qt::Horizontal, "最佳用时");
    tableModel->setHeaderData(7, Qt::Horizontal, "到期");
    tableModel->setHeaderData(8, Qt::Horizontal, "最近学习");
    for(int i = 9;i < 21;i++)
    {
        ui->tableView->hideColumn(i);
    }
}

void LearningDialog::clear_question_display()
{
    //clear items
    ui->textBrowser->setText("");
    QGridLayout *layout = (QGridLayout *)ui->groupBox->layout();
    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr)
    {
        //layout->removeWidget(child->widget());
        //layout->removeItem(child);
        delete child->widget();
        delete child;
    }
}

void LearningDialog::set_itmes_table(QString filter)
{
    tableModel->setFilter(filter);
}

void LearningDialog::set_question(int id)
{
    currentId = id;
    ui->idLabel->setText(QString("id:%1").arg(currentId));

    QString html = questionSql->read_questionHTML(id);
    QJsonArray array = questionSql->read_answerJSON(id);

    clear_question_display();
    ui->textBrowser->setHtml(html);

    //set answer
    QGridLayout *layout = (QGridLayout *)ui->groupBox->layout();

    //add items
    for(int i = 0;i < array.count();i++)
    {
        QLineEdit* newLineEdit = new QLineEdit(this);
        QLabel* newAnswerLabel = new QLabel(this);
        QLabel* newCheckLabel = new QLabel(this);
        int pixelWide = fm->horizontalAdvance(array[i].toString());

        newLineEdit->setMaximumWidth(pixelWide + 15);
        newAnswerLabel->setText(array[i].toString());
        newAnswerLabel->setMaximumWidth(pixelWide + 15);
        newCheckLabel->setText("错误");

        layout->addWidget(newLineEdit,i,0);
        layout->addWidget(newAnswerLabel,i,1);
        layout->addWidget(newCheckLabel,i,2);

        newAnswerLabel->hide();
        newCheckLabel->hide();
    }

    //add timeLabel
    timeLabel = new QLabel(this);
    QLabel* newGoodTimeLabel = new QLabel(this);
    QLabel* newCheckLabel = new QLabel(this);
    timeLabel->setText("00:00.00");
    QString timeString = ToolFunctions::sec2string(questionSql->get_goodTime(id));
    newGoodTimeLabel->setText(timeString);
    newCheckLabel->setText("良好");

    layout->addWidget(timeLabel,array.count(),0);
    layout->addWidget(newGoodTimeLabel,array.count(),1);
    layout->addWidget(newCheckLabel,array.count(),2);

    newCheckLabel->hide();

    //add spacer
    QSpacerItem* verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    layout->addItem(verticalSpacer,array.count()+1,0,1,3);

    layout->itemAt(0)->widget()->setFocus();
    //start timer
    start_timer();
}

bool LearningDialog::is_submited()
{
    return submited;
}

void LearningDialog::on_pushButton_clicked()
{
    stop_timer();
    if(tableModel->rowCount() == 0)
        return;

    if(currentId == -1)
        return;

    //两次提交切换题目、生成随机排序数
    if(submited)
    {
        //更新随机排序数
        if(ui->comboBox->currentText() == "随机排序")
        {
            tableModel->index(oldRow,0).data().toInt();
            int randNum = QRandomGenerator::global()->bounded(32768);
            questionSql->set_data(tableModel->index(oldRow,0).data().toInt(),"orderNum",randNum);
            ui->tableView->sortByColumn(21,Qt::AscendingOrder);
        }

        //切换题目
        int newRow = oldRow;
        if(tableModel->index(oldRow,0).data().toInt() == currentId)
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

    //主体
    submited = true;
    QGridLayout *layout = (QGridLayout *)ui->groupBox->layout();
    int row = 0;
    bool wrong = false;

    for(;row < (layout->count()-1)/3 -1;row++)
    {
        QLineEdit *lineEdit = (QLineEdit *)layout->itemAtPosition(row,0)->widget();
        QLabel *answerLabel = (QLabel *)layout->itemAtPosition(row,1)->widget();
        QLabel *checkLabel = (QLabel *)layout->itemAtPosition(row,2)->widget();

        if(lineEdit->text() == answerLabel->text())
            checkLabel->setText("正确");
        else
        {
            wrong = true;
            checkLabel->setText("错误");
        }

        lineEdit->setEnabled(false);
        answerLabel->show();
        checkLabel->show();
    }

    QLabel *timeLabel = (QLabel *)layout->itemAtPosition(row,0)->widget();
    QLabel *goodTimeLabel = (QLabel *)layout->itemAtPosition(row,1)->widget();
    QLabel *checkLabel = (QLabel *)layout->itemAtPosition(row,2)->widget();

    QTime goodTime = QTime::fromString(goodTimeLabel->text(),"mm:ss.zz");
    rating = FSRS::time2rating(time,goodTime);
    if(rating == "hard")
    {
        checkLabel->setText("困难");
    }
    else if(rating == "easy")
    {
        checkLabel->setText("简单");
    }
    else
    {
        checkLabel->setText("良好");
    }
    if(wrong)
    {
        rating = "wrong";
        time = QTime::fromMSecsSinceStartOfDay(0);
        checkLabel->setText("出错");
    }

    questionSql->update_question_state(currentId,time);
    checkLabel->show();

    oldRow = ui->tableView->currentIndex().row();
    set_itmes_table(tableModel->filter());
    if(oldRow >= tableModel->rowCount())
        oldRow = tableModel->rowCount() - 1;
    ui->tableView->selectRow(oldRow);

}

void LearningDialog::timerHandler()
{
    time = time.addMSecs(10);
    if(timeLabel)
    {
        timeLabel->setText(time.toString("mm:ss.zzz").chopped(1));
    }
}

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


void LearningDialog::on_LearningDialog_finished(int result)
{
    stop_timer();
    submited = true;
    currentId = -1;
}

QTime LearningDialog::getTime() const
{
    return time;
}

void LearningDialog::on_tableView_clicked(const QModelIndex &index)
{
    int id = index.siblingAtColumn(0).data().toInt();
    set_question(id);
}


void LearningDialog::on_comboBox_currentTextChanged(const QString &arg1)
{
    stop_timer();
    QSqlQuery query(questionSql->getDb());

    if(arg1 == "随机排序")
    {
        int rowCount = tableModel->rowCount();
        for(int i = 0;i < rowCount;i++)
        {
            ui->tableView->setSortingEnabled(false);
            int randNum = QRandomGenerator::global()->bounded(32768);
            int id = tableModel->index(i,0).data().toInt();
            questionSql->set_data(id,"orderNum",randNum);
        }
        ui->tableView->setSortingEnabled(true);
        ui->tableView->sortByColumn(21,Qt::AscendingOrder);
    }
    ui->tableView->clearSelection();
    clear_question_display();
}