#ifndef QUESTIONSQL_H
#define QUESTIONSQL_H

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

class QuestionSql : public QObject
{
    Q_OBJECT
public:
    explicit QuestionSql(QString fileName,QObject *parent = nullptr);

    //static
    static QMap<QString,int> headerColumnMap;

    //category
    void add_category(int id,QString name,int parent_id = 0);
    void del_category(int id);

    //question
    void add_question(int id,int categoryId,QString name);
    void del_question(int id);
    void update_question_state(int id);
    int count_total_questions(int categoryId);
    int count_total_questions_to_learn(int categoryId);
    QString get_category_condString(int categoryId);
    QString get_toLearn_condString(int categoryId);
    QString get_toLearn_condString(QString currentFilter);

    //construct
    void add_construct(int id);
    void del_construct(int id);
    void inc_construct_bind_count(int id);
    void dec_construct_bind_count(int id);
    void update_construct_state(int id, QTime myTime);

    //other

    QVariant get_value(QString table, int id, QString column);
    template<typename T>void set_value(QString table, int id, QString column, T value);
    QJsonArray read_answerJSON(int id);
    void write_answerJSON(int id, QJsonArray jsonArray);
    QString read_questionHTML(int id);
    void write_questionHTML(int id, QString html);
    QSqlDatabase getDb() const;
    int get_max_id(QString table);


    void resetFSRSData(int constructId); //重置FSRS数据8/12
private:
    QSqlDatabase db;
    QString dbFileName;
    void _del_category(int id, int parentId);
    //int tempId;
signals:
};

#endif // QUESTIONSQL_H
