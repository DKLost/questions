#include "ontologysql.h"

OntologySql::OntologySql(QString fileName, QObject *parent)
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
               "method INTEGER," //being
               "inject TEXT,"    //data {"type":content} 例如{"being":1221} 1221即being id
               "center INTEGER," //being
               "result INTEGER," //being
               "state TEXT,"     //For Fsrs
               "avg10Rating TEXT,"
               "goodTime TEXT,"
               "avg10Time TEXT,"
               "bestTime TEXT,"
               "nextDate TEXT," //For Fsrs
               "lastDate TEXT," //For Fsrs
               "lastD REAL,"    //For Fsrs
               "lastS REAL,"    //For Fsrs
               "time0 TEXT,"
               "time1 TEXT,"
               "time2 TEXT,"
               "time3 TEXT,"
               "time4 TEXT,"
               "time5 TEXT,"
               "time6 TEXT,"
               "time7 TEXT,"
               "time8 TEXT,"
               "time9 TEXT)");
}
