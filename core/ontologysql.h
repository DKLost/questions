#ifndef ONTOLOGYSQL_H
#define ONTOLOGYSQL_H

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

class OntologySql : public QObject
{
    Q_OBJECT
public:
    explicit OntologySql(QString fileName,QObject *parent = nullptr);


private:
    QSqlDatabase db;
    QString dbFileName;

signals:
};

#endif // ONTOLOGYSQL_H
