#include "questionsql.h"
#include "fsrs.h"
#include <QDate>
#include <QDir>
#include <QFile>
#include "toolfunctions.h"

QuestionSql::QuestionSql(QString fileName,QObject *parent)
    : QObject{parent}
{
    dbFileName = fileName;
    db = QSqlDatabase::addDatabase("QSQLITE","connection1");
    db.setDatabaseName(dbFileName);
    db.open();

    QSqlQuery query(db);

    //categories
    query.exec("CREATE TABLE IF NOT EXISTS categories(id INTEGER PRIMARY KEY,name TEXT,parentId INTEGER)");
    query.exec("INSERT INTO categories VALUES(0,'root',-1)");
    query.exec("INSERT INTO categories VALUES(1,'未分类',0)");

    //tags
    query.exec("CREATE TABLE IF NOT EXISTS tags(id INTEGER PRIMARY KEY,name TEXT,bestTime TEXT)");

    //questions
    query.exec("CREATE TABLE IF NOT EXISTS questions("
               "id INTEGER PRIMARY KEY,"
               "categoryId INTEGER,"
               "state TEXT,"
               "avg10Rating TEXT,"
               "avg10Time TEXT,"
               "goodTime TEXT,"
               "bestTime TEXT,"
               "nextDate TEXT,"
               "lastDate TEXT,"
               "lastD REAL,"
               "lastS REAL,"
               "time0 TEXT,"
               "time1 TEXT,"
               "time2 TEXT,"
               "time3 TEXT,"
               "time4 TEXT,"
               "time5 TEXT,"
               "time6 TEXT,"
               "time7 TEXT,"
               "time8 TEXT,"
               "time9 TEXT,"
               "orderNum INTEGER,"
               "tag TEXT)");
}

void QuestionSql::add_category(int id, QString name, int parentId)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO categories VALUES(?,?,?)");
    query.bindValue(0,id);
    query.bindValue(1,name);
    query.bindValue(2,parentId);
    query.exec();
}

void QuestionSql::del_category(int id)
{
    QSqlQuery query(db);
    query.prepare("SELECT parentId FROM categories WHERE id = ?");
    query.bindValue(0,id);
    query.exec();
    query.next();
    int parentId = query.value(0).toInt();
    if(parentId == 0)
        parentId = 1;
    _del_category(id,parentId);
}

void QuestionSql::_del_category(int id,int parentId)
{
    QSqlQuery query(db);

    query.exec("DELETE FROM categories WHERE id = ?");
    query.bindValue(0,id);
    query.exec();

    query.prepare("SELECT id FROM questions WHERE categoryId = ?");
    query.bindValue(0,id);
    query.exec();
    while(query.next())
    {
        set_question_categoryId(query.value(0).toInt(),parentId);
    }

    query.prepare("SELECT id FROM categories WHERE parentId = ?");
    query.bindValue(0,id);
    query.exec();
    while(query.next())
    {
        _del_category(query.value(0).toInt(),parentId);
    }
}

// void QuestionSql::add_tag(int id, QString name)
// {
//     QSqlQuery query(db);
//     query.prepare("INSERT INTO tag VALUES(?,?,?)");
//     query.bindValue(0,id);
//     query.bindValue(1,name);
//     query.bindValue(2,"23:59:59.00");
//     query.exec();
// }

// void QuestionSql::del_tag(int id)
// {
//     QSqlQuery query(db);
//     query.exec("DELETE FROM tags WHERE id = ?");
//     query.bindValue(0,id);
//     query.exec();

//     QString queryString = QString("SELECT * FROM questions WHERE tag GLOB '*?*'").arg(id);
//     query.exec(queryString);
//     while(query.next())
//     {
//         query.value(0).toInt();
//         QStringList list = query.value(2).toString().split(',');
//         list.remove(list.indexOf(id));
//     }
// }

void QuestionSql::add_question(int id, int categoryId)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO questions VALUES(?,?,?,?,?,?,?,?,?,?,?"
                  ",?,?,?,?,?,?,?,?,?,?,?,?)");
    query.bindValue(0,id); //id
    query.bindValue(1,categoryId); //categoryId
    query.bindValue(2,"new"); //state
    query.bindValue(3,""); //avg10Rating
    query.bindValue(4,""); //avg10Time
    query.bindValue(5,"00m:00s"); //goodTime
    query.bindValue(6,""); //bestTime
    query.bindValue(7,QDate::currentDate().toString("yyyy/MM/dd")); //nextDate
    query.bindValue(8,""); //lastDate
    query.bindValue(9,-1); //lastD
    query.bindValue(10,-1); //lastS

    for(int i = 0;i < 10;i++) //time0-9
        query.bindValue(11+i,"");

    query.bindValue(21,-1); //order
    query.bindValue(22,""); //tag
    query.exec();

    QString dirPath = QString("./data/%1").arg(id);
    QDir dir(".");
    dir.mkpath(dirPath);
    QFile fileQ(dirPath+"/question.html");
    QFile fileA(dirPath+"/answer.json");
    fileQ.open(QIODevice::ReadWrite);
    fileA.open(QIODevice::ReadWrite);

    write_answerJSON(id,QJsonArray{});
    fileQ.close();
    fileA.close();
}

void QuestionSql::del_question(int id)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM questions WHERE id = ?");
    query.bindValue(0,id);
    query.exec();
    QString dirPath = QString("./data/%1").arg(id);
    QDir dir(dirPath);
    dir.removeRecursively();
}

void QuestionSql::set_goodTime(int id, QTime goodTime)
{
    QSqlQuery query(db);
    QString goodTimeString = goodTime.toString("mm'm':ss's'");
    query.prepare("UPDATE questions SET goodTime = ? WHERE id = ?");
    query.bindValue(0,goodTimeString);
    query.bindValue(1,id);
    query.exec();
}


QSqlDatabase QuestionSql::getDb() const
{
    return db;
}

QString QuestionSql::read_questionHTML(int id)
{
    QString filePath = QString("./data/%1/question.html").arg(id);
    QFile file(filePath);
    file.open(QIODevice::ReadOnly);
    QString html = file.readAll();
    file.close();
    return html;
}

void QuestionSql::write_questionHTML(int id,QString html)
{
    QString filePath = QString("./data/%1/question.html").arg(id);
    QFile file(filePath);
    file.open(QIODevice::ReadWrite);
    file.resize(0);
    QTextStream fileIO(&file);
    fileIO << html;
    file.close();
}

void QuestionSql::set_question_categoryId(int id,int categoryId)
{
    QSqlQuery query(db);
    query.prepare("UPDATE questions SET categoryId = ? WHERE id = ?");
    query.bindValue(0,categoryId);
    query.bindValue(1,id);
    query.exec();
}

int QuestionSql::get_goodTime(int id)
{
    QSqlQuery query(db);
    query.prepare("SELECT goodTime FROM questions WHERE id = ?");
    query.bindValue(0,id);
    query.exec();
    if(query.next())
    {
        if(query.value(0).toString() == "")
            return 0;
        QString timeString = query.value(0).toString();
        QStringList list = timeString.split(':');
        if(list.size() != 2)
            return -1;
        int m = list[0].remove('m').toInt();
        int s = list[1].remove('s').toInt();
        int result =m*60 + s;
        return result;
    }
}

QJsonArray QuestionSql::read_answerJSON(int id)
{
    QString filePath = QString("./data/%1/answer.json").arg(id);
    QFile file(filePath);
    file.open(QIODevice::ReadOnly);
    QString json = file.readAll();
    file.close();
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    return document.array();
}

void QuestionSql::write_answerJSON(int id,QJsonArray jsonArray)
{
    QJsonDocument document(jsonArray);
    QString filePath = QString("./data/%1/answer.json").arg(id);
    QFile file(filePath);
    file.open(QIODevice::ReadWrite);
    file.resize(0);
    QTextStream fileIO(&file);
    fileIO << document.toJson();
    file.close();
}

QString QuestionSql::get_category_condString(int categoryId)
{
    QString condString = QString("categoryId = %1").arg(categoryId);
    QSqlQuery query(db);
    QList<int> ids{categoryId};
    for(int i = 0;i < ids.size();i++)
    {
        query.prepare("SELECT id FROM categories WHERE parentId = ?");
        query.bindValue(0,ids[i]);
        query.exec();
        while(query.next())
        {
            ids.append(query.value(0).toInt());
        }
        if(ids[i] != categoryId)
            condString.append(QString(" OR categoryId = %1").arg(ids[i]));
    }
    return condString;
}

QString QuestionSql::get_toLearn_condString(int categoryId)
{
    QString currentDate = QDate::currentDate().toString("yyyy/MM/dd");
    QString condString = QString("(%1 <= %2) AND %3")
                             .arg("nextDate")
                             .arg(QString("'%1'").arg(currentDate))
                             .arg(QString("(%1)").arg(get_category_condString(categoryId)));
    return condString;
}

int QuestionSql::count_total_questions_to_learn(int categoryId)
{
    QSqlQuery query(db);
    QString condString = get_toLearn_condString(categoryId);
    QString queryString = QString("SELECT COUNT(*) FROM questions WHERE %1").arg(condString);
    query.exec(queryString);
    query.next();
    int qCount = query.value(0).toInt();

    return qCount;
}

int QuestionSql::count_total_questions(int categoryId)
{
    QSqlQuery query(db);
    QString condString = get_category_condString(categoryId);
    QString queryString = QString("SELECT COUNT(*) FROM questions WHERE %1").arg(condString);
    query.exec(queryString);
    query.next();
    int qCount = query.value(0).toInt();

    return qCount;
}

QVariant QuestionSql::get_data(int id,QString name)
{
    QSqlQuery query(db);
    QString queryString = QString("SELECT %1 FROM questions WHERE id = %2").arg(name).arg(id);
    query.exec(queryString);
    if(query.next())
    {
        return query.value(0);
    }
    else
        return NULL;

}


void QuestionSql::update_question_state(int id,QTime myTime)
{
    QSqlQuery query(db);

    QTime goodTime = QTime::fromString(get_data(id,"goodTime").toString(),"mm'm':ss's'");
    QString rating = FSRS::time2rating(myTime,goodTime);
    QString state = get_data(id,"state").toString();
    if(goodTime.msecsSinceStartOfDay() == 0)
    {
        if(rating != "wrong")
        {
            myTime = QTime::fromMSecsSinceStartOfDay(myTime.msecsSinceStartOfDay()*1.5);
            set_data(id,"goodTime",myTime.toString("mm'm':ss's'"));
        }
        return;
    }
    double d = 0.0;
    double s = 0.0;
    int interval = 0;
    if(state == "new")
    {
        d = FSRS::init_difficulty(FSRS::rating[rating]);
        s = FSRS::init_stability(FSRS::rating[rating]);
        interval = FSRS::next_interval(s);

        if(rating == "wrong") set_data(id,"state","learning");
        if(rating == "hard") set_data(id,"state","learning");
        if(rating == "good") set_data(id,"state","learning");
        if(rating == "easy")
        {
            set_data(id,"state","review");
            double goodS = FSRS::init_stability(FSRS::rating["good"]);
            int goodInterval = FSRS::next_interval(goodS);
            interval = qMax(interval,goodInterval + 1);
        }
    }else if(state == "learning")
    {
        double lastD = get_data(id,"lastD").toDouble();
        double lastS = get_data(id,"lastS").toDouble();
        d = FSRS::next_difficulty(lastD,FSRS::rating[rating]);
        s = FSRS::next_short_term_stability(lastS,FSRS::rating[rating]);
        interval = FSRS::next_interval(s);

        if(rating == "wrong") ;
        if(rating == "hard") ;
        if(rating == "good") set_data(id,"state","review");
        if(rating == "easy")
        {
            set_data(id,"state","review");
            double goodS = FSRS::next_short_term_stability(lastS,FSRS::rating["good"]);
            int goodInterval = FSRS::next_interval(goodS);
            interval = qMax(interval,goodInterval + 1);
        }
    }else if(state == "review")
    {
        QDate lastDate = QDate::fromString(get_data(id,"lastDate").toString(),"yyyy/MM/dd");
        int elapsedDays = lastDate.daysTo(QDate::currentDate());
        double lastD = get_data(id,"lastD").toDouble();
        double lastS = get_data(id,"lastS").toDouble();
        double retrievability = FSRS::forgetting_curve(elapsedDays,lastS);
        d = FSRS::next_difficulty(lastD,FSRS::rating[rating]);
        s = FSRS::next_recall_stability(lastD,lastS,retrievability,FSRS::rating[rating]);
        interval = FSRS::next_interval(s);

        if(rating == "wrong")
        {
            set_data(id,"state","learning");
            s = FSRS::next_forget_stability(lastD,lastS,FSRS::rating[rating]);
            interval = FSRS::next_interval(s);
        }
        if(rating == "hard")
        {
            set_data(id,"state","review");
            double goodS = FSRS::next_short_term_stability(lastS,FSRS::rating["good"]);
            int goodInterval = FSRS::next_interval(goodS);
            interval = qMin(interval,goodInterval);
        }
        if(rating == "good")
        {
            set_data(id,"state","review");
            double hardS = FSRS::next_short_term_stability(lastS,FSRS::rating["hard"]);
            int hardInterval = FSRS::next_interval(hardS);
            interval = qMax(interval,hardInterval + 1);
        }
        if(rating == "easy")
        {
            set_data(id,"state","review");
            double goodS = FSRS::next_short_term_stability(lastS,FSRS::rating["good"]);
            int goodInterval = FSRS::next_interval(goodS);
            interval = qMax(interval,goodInterval + 1);
        }
    }

    set_data(id,"lastD",d);
    set_data(id,"lastS",s);
    set_data(id,"lastDate",QDate::currentDate().toString("yyyy/MM/dd"));
    set_data(id,"nextDate",QDate::currentDate().addDays(interval - 1).toString("yyyy/MM/dd"));

    if(rating == "wrong")
        return;

    QString newTime = myTime.toString("mm:ss.zzz").chopped(1);
    QString oldBestTime = get_data(id,"bestTime").toString();
    if(oldBestTime == "" || newTime < oldBestTime)
    {
        set_data(id,"bestTime",newTime);
    }

    int count = 1;
    int tSum = myTime.msecsSinceStartOfDay() ;
    for(int i = 9;i > 0;i--)
    {
        QString tNameBefore = QString("time%1").arg(i-1);
        QString tValueBefore = get_data(id,tNameBefore).toString();
        QString tName = QString("time%1").arg(i);
        QString tValue = get_data(id,tName).toString();
        set_data(id,tName,tValueBefore);
        if(tValue == "")
            continue;
        count++;
        tSum += QTime::fromString(tValue,"mm:ss.zz").msecsSinceStartOfDay();
    }
    set_data(id,"time0",newTime);

    int tAvg = tSum / count;
    QTime newAvg10Time = QTime::fromMSecsSinceStartOfDay(tAvg);
    set_data(id,"avg10Time",newAvg10Time.toString("mm:ss.zzz").chopped(1));
    set_data(id,"avg10Rating",FSRS::time2rating(newAvg10Time,goodTime));
}


