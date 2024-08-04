#include "questiontageditdialog.h"
#include "ui_questiontageditdialog.h"


QuestionTagEditDialog::QuestionTagEditDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::QuestionTagEditDialog)
{
    ui->setupUi(this);
    tagListModel = new QStringListModel(this);
    tagListModel->setStringList(tagList);
    ui->listView->setModel(tagListModel);
}

QuestionTagEditDialog::~QuestionTagEditDialog()
{
    delete ui;
}

void QuestionTagEditDialog::set_question(int newId)
{
    qId = newId;
    //set tag list view
    QSqlQuery query(QSqlDatabase::database("connection1"));
    query.prepare("SELECT tag FROM questions WHERE id = ?");
    query.bindValue(0,qId);
    query.exec();
    query.next();


    tagList = query.value(0).toString().split(',',Qt::SkipEmptyParts);
    for(int i = 0;i < tagList.count();i++)
    {
        query.prepare("SELECT name FROM tags WHERE id = ?");
        query.bindValue(0,tagList[i]);
        query.exec();
        query.next();
        tagList[i] = query.value(0).toString();
    }

    tagListModel->setStringList(tagList);

    //set tag combobox
    ui->tagComboBox->clear();
    query.exec("SELECT name FROM tags");
    while(query.next())
    {
        ui->tagComboBox->addItem(query.value(0).toString());
    }

    //set qId label
    ui->idLabel->setText(QString("id:%1").arg(qId));
}

void QuestionTagEditDialog::on_addButton_clicked()
{
    QSqlQuery query(QSqlDatabase::database("connection1"));
    QString tagName = ui->tagComboBox->currentText();

    //get tag id by tag name
    query.prepare("SELECT id FROM tags WHERE name = ?");
    query.bindValue(0,tagName);
    query.exec();
    query.next();
    int tagId = query.value(0).toInt();

    //add new tag to current question
    QString queryString = QString("SELECT tag FROM questions WHERE id = %1").arg(qId);
    query.exec(queryString);
    if(query.next())
    {
        QStringList list = query.value(0).toString().split(',',Qt::SkipEmptyParts);
        qDebug() << list;
        if(list.indexOf(QString::number(tagId)) != -1)
            return;

        list.append(QString::number(tagId));
        QString newTag = list.join(',');
        query.prepare("UPDATE questions SET tag = ? WHERE id = ?");
        query.bindValue(0,newTag);
        query.bindValue(1,qId);
        query.exec();

        //add new tag to list view
        int rowCount = tagListModel->rowCount();
        tagListModel->insertRows(rowCount,1);
        QModelIndex index = tagListModel->index(rowCount);
        tagListModel->setData(index,tagName);

        //reset tag best time
        query.prepare("UPDATE tags SET bestTime = ? WHERE id = ?");
        query.bindValue(0,"23:59:59.99");
        query.bindValue(1,tagId);
        query.exec();
    }
}

void QuestionTagEditDialog::on_delButton_clicked()
{
    QSqlQuery query(QSqlDatabase::database("connection1"));
    QString tagName = ui->tagComboBox->currentText();

    //get tag id by tag name
    query.prepare("SELECT id FROM tags WHERE name = ?");
    query.bindValue(0,tagName);
    query.exec();
    query.next();
    int tagId = query.value(0).toInt();

    //del the tag from current question
    QString queryString = QString("SELECT tag FROM questions WHERE id = %1 "
                                  "AND ((tag GLOB '*,%2') OR (tag GLOB '*,%2,*') OR (tag GLOB '%2,*') OR (tag GLOB '%2'))").arg(qId).arg(tagId);
    query.exec(queryString);
    if(query.next())
    {
        QStringList list = query.value(0).toString().split(',',Qt::SkipEmptyParts);
        list.remove(list.indexOf(QString::number(tagId)));
        QString newTag = list.join(',');
        query.prepare("UPDATE questions SET tag = ? WHERE id = ?");
        query.bindValue(0,newTag);
        query.bindValue(1,qId);
        query.exec();

        //remove tag from list view
        for(int i = 0;i < tagListModel->rowCount();i++)
        {
            if(tagName == tagListModel->index(i).data().toString())
            {
                tagListModel->removeRow(i);
                break;
            }
        }

        //reset tag best time
        query.prepare("UPDATE tags SET bestTime = ? WHERE id = ?");
        query.bindValue(0,"23:59:59.99");
        query.bindValue(1,tagId);
        query.exec();
    }
}

void QuestionTagEditDialog::on_listView_clicked(const QModelIndex &index)
{
    on_listView_activated(index);
}


void QuestionTagEditDialog::on_listView_activated(const QModelIndex &index)
{
    ui->tagComboBox->setCurrentText(index.data().toString());
}

