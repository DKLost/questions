#include "beingconstructsql.h"

QMap<QString,int> BeingConstructSql::being = {
    {"*",0},
    {"@",1},
    {"←",2},
    {"~",3},
    {"=",4},
    {"→",100}
};

BeingConstructSql::BeingConstructSql(QString fileName, QObject *parent)
    : QObject{parent}
{
    dbFileName = fileName;
    db = QSqlDatabase::addDatabase("QSQLITE","connection2");
    db.setDatabaseName(dbFileName);
    db.open();

    QSqlQuery query(db);
    //beings
    query.exec("CREATE TABLE IF NOT EXISTS beings("
               "id INTEGER PRIMARY KEY)");

    //constructs
    query.exec("CREATE TABLE IF NOT EXISTS constructs("
               "id INTEGER PRIMARY KEY,"
               "time TEXT,"   //Time
               "method INTEGER,"   //being
               "center INTEGER,"   //being
               "inject INTEGER,"   //being
               "result TEXT)"   //data {"type":content} 例如{"being":1221} 1221即being id
               );

    //constructs
    query.exec("CREATE TABLE IF NOT EXISTS constructs("
               "id INTEGER PRIMARY KEY,"
               "method INTEGER,"   //being
               "center INTEGER,"   //being
               "inject INTEGER,"   //being
               "result TEXT)"   //data {"type":content} 例如{"being":1221} 1221即being id
               );

    //phenomena
    query.exec("CREATE TABLE IF NOT EXISTS phenomena("
               "id INTEGER PRIMARY KEY,"
               "relate INTEGER,"   //being
               "source INTEGER,"   //being
               "target INTEGER"   //being
               );
    add_being(being["{}"]);
    add_being(being["→"]);
    add_being(being["~"]);
    add_being(being["←"]);
    add_construct(0,being["→"],being["~"],being["←"],being["←"]);
    add_construct(1,being["←"],being["~"],being["←"],being["→"]);
    add_construct(2,being["*"],being["@"],being["←"],being["*"]);

    _add_construct(0,being["*"],being["*"],being["*"],R"({"being" : 1112})");
    _add_construct(0,being["←"],being["@"],1112,R"({"being" : 1112})");


}

void BeingConstructSql::add_being(int id)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO beings VALUES(:id)");
    query.bindValue(":id",id);
    query.exec();
}

void BeingConstructSql::_add_construct(int id,int methodId, int centerId, int injectId, QString resultData)
{
    QSqlQuery query(db);

    query.prepare("INSERT INTO constructs VALUES(:id,:method,:center,:inject,:result)");
    query.bindValue(":id",id);
    query.bindValue(":method",methodId);
    query.bindValue(":center",centerId);
    query.bindValue(":inject",injectId);
    query.bindValue(":result",resultData);
    query.exec();
}

void BeingConstructSql::_add_construct(int id,int methodId, int centerId, int injectId, int resultId)
{
    auto resultData = QString("{\"being\" : %1}").arg(resultId);
    _add_construct(id,methodId,centerId,injectId,resultData);
}

void BeingConstructSql::add_construct(int id, QString resultData, int centerId, int methodId, int injectId)
{
    _add_construct(id,methodId,centerId,injectId,resultData);
}

void BeingConstructSql::add_construct(int id, int resultId, int centerId, int methodId, int injectId)
{
    _add_construct(id,methodId,centerId,injectId,resultId);
}


int BeingConstructSql::get_max_id(QString table)
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
