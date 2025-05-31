#include "core/questionsql.h"
#include "core/fsrs.h"
#include "toolfunctions.h"

QuestionSql::QuestionSql(QString fileName,QObject *parent)
    : QObject{parent}
{
    dbFileName = fileName;
    db = QSqlDatabase::addDatabase("QSQLITE","connection1");
    db.setDatabaseName(dbFileName);
    db.open();

    QSqlQuery query(db);

    //beings
    query.exec("CREATE TABLE IF NOT EXISTS beings("
               "id INTEGER PRIMARY KEY,"
               "name TEXT,"
               "constructList TEXT)");

    //constructs
    query.exec("CREATE TABLE IF NOT EXISTS constructs("
               "id INTEGER PRIMARY KEY,"
               "center INTEGER,"   //being
               "method INTEGER,"   //being
               "inject INTEGER,"   //being
               "result INTEGER,"   //data {"type":content} 例如{"being":1221} 1221即being id
               "state TEXT,"       //for fsrs
               "avg10Rating TEXT,"
               "goodTime TEXT,"
               "avg10Time TEXT,"
               "bestTime TEXT,"
               "nextDate TEXT,"    //for fsrs
               "lastDate TEXT,"    //for fsrs
               "lastD REAL,"       //for fsrs
               "lastS REAL,"       //for fsrs
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
               "bindCount INTEGER,"
               "bindQuestions TEXT)");

    //categories
    query.exec("CREATE TABLE IF NOT EXISTS categories("
               "id INTEGER PRIMARY KEY,"
               "name TEXT,"
               "parentId INTEGER)");
    query.exec("INSERT INTO categories VALUES(0,'root',-1)");
    query.exec("INSERT INTO categories VALUES(1,'未分类',0)");


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
               "orderNum INTEGER)");
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

//question
void QuestionSql::add_question(int id, int categoryId,QString name)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO questions VALUES(?,?,?,?,?,?,?,?,?,?,?)");
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
    const QJsonArray array =read_answerJSON(id);
    for(auto item : array)
    {
        int aId = item.toObject().value("id").toInt();
        del_construct(aId);
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

//constructs
void QuestionSql::add_construct(int id)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO constructs VALUES(:id,:center,:method,:inject,:result,"
                  ":state,:avg10Rating,:goodTime,:avg10Time,:bestTime,:nextDate,:lastDate,:lastD,:lastS,"
                  ":time0,:time1,:time2,:time3,:time4,:time5,:time6,:time7,:time8,:time9,"
                  ":bindCount,:bindQuestions)");
    query.bindValue(":id",id);
    query.bindValue(":center",-1);
    query.bindValue(":method",-1);
    query.bindValue(":inject",-1);
    query.bindValue(":result","");
    query.bindValue(":state","new");
    query.bindValue(":avg10Rating","");
    query.bindValue(":goodTime","00m:00s");
    query.bindValue(":avg10Time","");
    query.bindValue(":bestTime","");
    query.bindValue(":nextDate",QDate::currentDate().toString("yyyy/MM/dd"));
    query.bindValue(":lastDate","");
    query.bindValue(":lastD",-1);
    query.bindValue(":lastS",-1);
    for(int i = 0;i < 10;i++) //time0-9
        query.bindValue(QString{":time"}+QString::number(i),"");
    query.bindValue(":bindCount",0);
    query.bindValue(":bindQuestions","");
    query.exec();
}
void QuestionSql::del_construct(int id)
{
    QSqlQuery query(db);
    int bindCount = get_value("constructs",id,"bindCount").toInt();
    if(bindCount > 1)
        dec_construct_bind_count(id);
    else {
        query.prepare("DELETE FROM constructs WHERE id = ?");
        query.bindValue(0,id);
        query.exec();
    }
}
void QuestionSql::inc_construct_bind_count(int aId)
{
    int bindCount = get_value("constructs",aId,"bindCount").toInt();
    set_value("constructs",aId,"bindCount",bindCount+1);
}
void QuestionSql::dec_construct_bind_count(int aId)
{
    int bindCount = get_value("constructs",aId,"bindCount").toInt();
    set_value("constructs",aId,"bindCount",bindCount-1);
}
void QuestionSql::update_construct_state(int id,QTime myTime)
{
    QSqlQuery query(db);

    QTime goodTime = QTime::fromString(get_value("constructs",id,"goodTime").toString(),"mm'm':ss's'");
    QDate lastDate = QDate::fromString(get_value("constructs",id,"lastDate").toString(),"yyyy/MM/dd");

    QString rating = FSRS::time2rating(myTime,goodTime);
    QString state = get_value("constructs",id,"state").toString();
    double d = get_value("constructs",id,"lastD").toDouble();
    double s = get_value("constructs",id,"lastS").toDouble();
    int interval = 0;
    int elapsedDays = 0;
    if(lastDate.isValid())
        elapsedDays = lastDate.daysTo(QDate::currentDate());

    if(goodTime.msecsSinceStartOfDay() == 0)
    {
        if(rating != "wrong")
        {
            myTime = QTime::fromMSecsSinceStartOfDay(myTime.msecsSinceStartOfDay()*1.5);
            set_value("constructs",id,"goodTime",myTime.toString("mm'm':ss's'"));
        }
        return;
    }

    interval = FSRS::next_state(rating,elapsedDays,state,d,s);

    set_value("constructs",id,"state",state);
    set_value("constructs",id,"lastD",d);
    set_value("constructs",id,"lastS",s);
    set_value("constructs",id,"lastDate",QDate::currentDate().toString("yyyy/MM/dd"));
    set_value("constructs",id,"nextDate",QDate::currentDate().addDays(interval).toString("yyyy/MM/dd"));

    if(rating == "wrong")
        return;

    QString newTime = myTime.toString("mm:ss.zzz").chopped(1);
    QString oldBestTime = get_value("constructs",id,"bestTime").toString();
    if(oldBestTime == "" || newTime < oldBestTime)
    {
        set_value("constructs",id,"bestTime",newTime);
    }

    int count = 1;
    int tSum = myTime.msecsSinceStartOfDay() ;
    for(int i = 9;i > 0;i--)
    {
        QString tNameBefore = QString("time%1").arg(i-1);
        QString tValueBefore = get_value("constructs",id,tNameBefore).toString();
        QString tName = QString("time%1").arg(i);
        QString tValue = get_value("constructs",id,tName).toString();
        set_value("constructs",id,tName,tValueBefore);
        if(tValue == "")
            continue;
        count++;
        tSum += QTime::fromString(tValue,"mm:ss.zz").msecsSinceStartOfDay();
    }
    set_value("constructs",id,"time0",newTime);

    int tAvg = tSum / count;
    QTime newAvg10Time = QTime::fromMSecsSinceStartOfDay(tAvg);
    set_value("constructs",id,"avg10Time",newAvg10Time.toString("mm:ss.zzz").chopped(1));
    set_value("constructs",id,"avg10Rating",FSRS::time2rating(newAvg10Time,goodTime));
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
        QString aState = get_value("constructs",aId,"state").toString();
        QTime aGoodTime = QTime::fromString(get_value("constructs",aId,"goodTime").toString(),"mm'm':ss's'");
        QTime aAvg10Time = QTime::fromString(get_value("constructs",aId,"avg10Time").toString(),"mm:ss.zz");
        QTime aBestTime = QTime::fromString(get_value("constructs",aId,"bestTime").toString(),"mm:ss.zz");
        QDate aNextDate = QDate::fromString(get_value("constructs",aId,"nextDate").toString(),"yyyy/MM/dd");
        QDate aLastDate = QDate::fromString(get_value("constructs",aId,"lastDate").toString(),"yyyy/MM/dd");

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

//ui
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
