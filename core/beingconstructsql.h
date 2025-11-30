#ifndef BEINGCONSTRUCTSQL_H
#define BEINGCONSTRUCTSQL_H

#include <QObject>
#include <QSql>
#include <QSqlQueryModel>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QDate>
#include <QMap>

class BeingConstructSql : public QObject
{
    Q_OBJECT
public:
    explicit BeingConstructSql(QString fileName,QObject *parent = nullptr);
    void add_being(int id);
    void _add_construct(int id,int methodId,int centerId,int injectId,QString resultData);
    void _add_construct(int id, int methodId, int centerId, int injectId, int resultId);
    void add_construct(int id,QString resultData,int centerId,int methodId,int injectId);
    void add_construct(int id, int resultId,int centerId,int methodId, int injectId);

    int get_max_id(QString table);
    static QMap<QString,int> being;

private:
    QSqlDatabase db;
    QString dbFileName;
signals:
};

#endif // BEINGCONSTRUCTSQL_H
