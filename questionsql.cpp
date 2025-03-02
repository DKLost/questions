#include "questionsql.h"
#include "fsrs.h"
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

    //answers
    query.exec("CREATE TABLE IF NOT EXISTS answers("
               "id INTEGER PRIMARY KEY,"
               "state TEXT,"
               "avg10Rating TEXT,"
               "goodTime TEXT,"
               "avg10Time TEXT,"
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
               "bindCount INTEGER)");

    //questions
    query.exec("CREATE TABLE IF NOT EXISTS questions("
               "id INTEGER PRIMARY KEY,"
               "categoryId INTEGER,"
               "name TEXT,"
               "state TEXT,"
               "avg10Rating TEXT,"
               "goodTime TEXT,"
               "avg10Time TEXT,"
               "bestTime TEXT,"
               "nextDate TEXT,"
               "lastDate TEXT,"
               "orderNum INTEGER,"
               "tag TEXT)");
}

//category
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
        set_value("categories",query.value(0).toInt(),"categoryId",parentId);
    }

    query.prepare("SELECT id FROM categories WHERE parentId = ?");
    query.bindValue(0,id);
    query.exec();
    while(query.next())
    {
        _del_category(query.value(0).toInt(),parentId);
    }
}

//tag
void QuestionSql::add_tag(int id, QString name)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO tags VALUES(?,?,?)");
    query.bindValue(0,id);
    query.bindValue(1,name);
    query.bindValue(2,"23:59:59.99");
    query.exec();
}
void QuestionSql::del_tag(int id)
{
    QSqlQuery query(db);
    QSqlQuery query2(db);

    query.exec("DELETE FROM tags WHERE id = ?");
    query.bindValue(0,id);
    query.exec();

    QString queryString = QString("SELECT id, tag FROM questions "
                                  "WHERE (tag GLOB '*,%1') OR (tag GLOB '*,%1,*') OR (tag GLOB '%1,*') OR (tag GLOB '%1')").arg(id);
    query.exec(queryString);
    while(query.next())
    {
        int qId = query.value(0).toInt();
        QStringList tagList = query.value(1).toString().split(',',Qt::SkipEmptyParts);
        tagList.remove(tagList.indexOf(QString::number(id)));
        query2.prepare("UPDATE questions SET tag = ? WHERE id = ?");
        query2.bindValue(0,tagList.join(','));
        query2.bindValue(1,qId);
        query2.exec();
    }
}

//answer
void QuestionSql::add_answer(int id)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO answers VALUES(?,?,?,?,?,?,?,?,?,?"
                  ",?,?,?,?,?,?,?,?,?,?,?)");
    query.bindValue(0,id); //id
    query.bindValue(1,"new"); //state
    query.bindValue(2,""); //avg10Rating
    query.bindValue(3,"00m:00s"); //goodTime
    query.bindValue(4,""); //avg10Time
    query.bindValue(5,""); //bestTime
    query.bindValue(6,QDate::currentDate().toString("yyyy/MM/dd")); //nextDate
    query.bindValue(7,""); //lastDate
    query.bindValue(8,-1); //lastD
    query.bindValue(9,-1); //lastS
    for(int i = 0;i < 10;i++) //time0-9
        query.bindValue(10+i,"");
    query.bindValue(20,0);
    query.exec();
}
void QuestionSql::del_answer(int id)
{
    QSqlQuery query(db);
    int bindCount = get_value("answers",id,"bindCount").toInt();
    if(bindCount > 1)
        dec_answer_bind_count(id);
    else {
        query.prepare("DELETE FROM answers WHERE id = ?");
        query.bindValue(0,id);
        query.exec();
    }

}
void QuestionSql::inc_answer_bind_count(int aId)
{
    int bindCount = get_value("answers",aId,"bindCount").toInt();
    set_value("answers",aId,"bindCount",bindCount+1);
}
void QuestionSql::dec_answer_bind_count(int aId)
{
    int bindCount = get_value("answers",aId,"bindCount").toInt();
    set_value("answers",aId,"bindCount",bindCount-1);
}

//question
void QuestionSql::add_question(int id, int categoryId,QString name)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO questions VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
    query.bindValue(0,id); //id
    query.bindValue(1,categoryId); //categoryId
    query.bindValue(2,name); //name
    query.bindValue(3,"new"); //state
    query.bindValue(4,""); //avg10Rating
    query.bindValue(5,"00m:00s"); //goodTime
    query.bindValue(6,""); //avg10Time
    query.bindValue(7,""); //bestTime
    query.bindValue(8,QDate::currentDate().toString("yyyy/MM/dd")); //nextDate
    query.bindValue(9,""); //lastDate
    query.bindValue(10,-1); //order
    query.bindValue(11,""); //tag
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
    QJsonArray array =read_answerJSON(id);
    for(auto item : array)
    {
        int aId = item.toObject().value("id").toInt();
        del_answer(aId);
    }
    query.prepare("DELETE FROM questions WHERE id = ?");
    query.bindValue(0,id);
    query.exec();
    QString dirPath = QString("./data/%1").arg(id);
    QDir dir(dirPath);
    dir.removeRecursively();
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
QString QuestionSql::get_toLearn_condString(QString currentFilter)
{
    QString currentDate = QDate::currentDate().toString("yyyy/MM/dd");
    QString condString = QString("(%1 <= %2) AND %3")
                             .arg("nextDate")
                             .arg(QString("'%1'").arg(currentDate))
                             .arg(QString("(%1)").arg(currentFilter));
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

//other
QVariant QuestionSql::get_value(QString table,int id,QString column)
{
    QSqlQuery query(db);
    QString queryString = QString("SELECT %1 FROM %2 WHERE id = %3").arg(column).arg(table).arg(id);
    query.exec(queryString);
    if(query.next())
    {
        return query.value(0);
    }
    else
        return NULL;
}
template<typename T>void QuestionSql::set_value(QString table,int id,QString column,T value)
{
    QSqlQuery query(db);
    QString queryString = QString("UPDATE %1 SET %2 = ? WHERE id = %3").arg(table).arg(column).arg(id);
    query.prepare(queryString);
    query.bindValue(0,value);
    query.exec();
}
QJsonArray QuestionSql::read_answerJSON(int qId)
{
    QString filePath = QString("./data/%1/answer.json").arg(qId);
    QFile file(filePath);
    file.open(QIODevice::ReadOnly);
    QString json = file.readAll();
    file.close();
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    return document.array();
}
void QuestionSql::write_answerJSON(int qId,QJsonArray jsonArray)
{
    QJsonDocument document(jsonArray);
    QString filePath = QString("./data/%1/answer.json").arg(qId);
    QFile file(filePath);
    file.open(QIODevice::ReadWrite);
    file.resize(0);
    QTextStream fileIO(&file);
    fileIO << document.toJson();
    file.close();
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
QSqlDatabase QuestionSql::getDb() const
{
    return db;
}
int QuestionSql::get_max_id(QString table)
{
    int id = 0;
    QSqlQuery query(db);
    query.exec(QString("SELECT MAX(id) FROM %1").arg(table));
    if(query.next())
    {
        id = query.value(0).toInt();
    }
    return id;
}


//timeStringToInt => QTime::fromString(timeString,"mm'm':ss's'").second();
//void set_goodTime(int id, QTime goodTime) => set_value("questions",id,"goodTime",goodTime.toString("mm'm':ss's'"));
//void set_question_categoryId(int id,int categoryId) => set_value("questions",id,"categoryId",categoryId);
//int get_goodTime(int id) => timeStringToInt(get_value("questions",id,"goodTime"));


void QuestionSql::update_answer_state(int id,QTime myTime)
{
    QSqlQuery query(db);

    QTime goodTime = QTime::fromString(get_value("answers",id,"goodTime").toString(),"mm'm':ss's'");
    QDate lastDate = QDate::fromString(get_value("answers",id,"lastDate").toString(),"yyyy/MM/dd");

    QString rating = FSRS::time2rating(myTime,goodTime);
    QString state = get_value("answers",id,"state").toString();
    double d = get_value("answers",id,"lastD").toDouble();
    double s = get_value("answers",id,"lastS").toDouble();
    int interval = 0;
    int elapsedDays = 0;
    if(lastDate.isValid())
        elapsedDays = lastDate.daysTo(QDate::currentDate());

    if(goodTime.msecsSinceStartOfDay() == 0)
    {
        if(rating != "wrong")
        {
            myTime = QTime::fromMSecsSinceStartOfDay(myTime.msecsSinceStartOfDay()*1.5);
            set_value("answers",id,"goodTime",myTime.toString("mm'm':ss's'"));
        }
        return;
    }

    interval = FSRS::next_state(rating,elapsedDays,state,d,s);

    set_value("answers",id,"state",state);
    set_value("answers",id,"lastD",d);
    set_value("answers",id,"lastS",s);
    set_value("answers",id,"lastDate",QDate::currentDate().toString("yyyy/MM/dd"));
    set_value("answers",id,"nextDate",QDate::currentDate().addDays(interval).toString("yyyy/MM/dd"));

    if(rating == "wrong")
        return;

    QString newTime = myTime.toString("mm:ss.zzz").chopped(1);
    QString oldBestTime = get_value("answers",id,"bestTime").toString();
    if(oldBestTime == "" || newTime < oldBestTime)
    {
        set_value("answers",id,"bestTime",newTime);
    }

    int count = 1;
    int tSum = myTime.msecsSinceStartOfDay() ;
    for(int i = 9;i > 0;i--)
    {
        QString tNameBefore = QString("time%1").arg(i-1);
        QString tValueBefore = get_value("answers",id,tNameBefore).toString();
        QString tName = QString("time%1").arg(i);
        QString tValue = get_value("answers",id,tName).toString();
        set_value("answers",id,tName,tValueBefore);
        if(tValue == "")
            continue;
        count++;
        tSum += QTime::fromString(tValue,"mm:ss.zz").msecsSinceStartOfDay();
    }
    set_value("answers",id,"time0",newTime);

    int tAvg = tSum / count;
    QTime newAvg10Time = QTime::fromMSecsSinceStartOfDay(tAvg);
    set_value("answers",id,"avg10Time",newAvg10Time.toString("mm:ss.zzz").chopped(1));
    set_value("answers",id,"avg10Rating",FSRS::time2rating(newAvg10Time,goodTime));
}

void QuestionSql::update_question_state(int id)
{
    QSqlQuery query(db);
    QString state = "review";
    QString avg10Rating = "";
    QTime goodTime = QTime::fromMSecsSinceStartOfDay(0);
    QTime avg10Time = QTime::fromMSecsSinceStartOfDay(0);
    QTime bestTime = QTime::fromMSecsSinceStartOfDay(0);
    QDate nextDate = QDate::currentDate().addYears(100);
    QDate lastDate = QDate::currentDate();

    QJsonArray answerArray = read_answerJSON(id);
    for(auto a : answerArray)
    {
        int aId = a.toObject().value("id").toInt();
        QString aState = get_value("answers",aId,"state").toString();
        QTime aGoodTime = QTime::fromString(get_value("answers",aId,"goodTime").toString(),"mm'm':ss's'");
        QTime aAvg10Time = QTime::fromString(get_value("answers",aId,"avg10Time").toString(),"mm:ss.zz");
        QTime aBestTime = QTime::fromString(get_value("answers",aId,"bestTime").toString(),"mm:ss.zz");
        QDate aNextDate = QDate::fromString(get_value("answers",aId,"nextDate").toString(),"yyyy/MM/dd");
        QDate aLastDate = QDate::fromString(get_value("answers",aId,"lastDate").toString(),"yyyy/MM/dd");

        if(FSRS::state[aState] < FSRS::state[state]) state = aState;
        goodTime = goodTime.addMSecs(aGoodTime.msecsSinceStartOfDay());
        avg10Time = avg10Time.addMSecs(aAvg10Time.msecsSinceStartOfDay());
        bestTime = bestTime.addMSecs(aBestTime.msecsSinceStartOfDay());
        if(aNextDate < nextDate) nextDate = aNextDate;
        if(aLastDate < lastDate) lastDate = aLastDate;

    }
    if(answerArray.empty())
    {
        state = "new";
        nextDate = QDate::currentDate();
    }
    else
    {
        avg10Rating = FSRS::time2rating(avg10Time,goodTime);
    }

    set_value("questions",id,"state",state);
    set_value("questions",id,"avg10Rating",avg10Rating);
    set_value("questions",id,"avg10Time",avg10Time.toString("mm:ss.zzz").chopped(1));
    set_value("questions",id,"goodTime",goodTime.toString("mm'm':ss's'"));
    set_value("questions",id,"bestTime",bestTime.toString("mm:ss.zzz").chopped(1));
    set_value("questions",id,"nextDate",nextDate.toString("yyyy/MM/dd"));
    set_value("questions",id,"lastDate",lastDate.toString("yyyy/MM/dd"));
}


