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
#include <QStringList>


class QuestionSql : public QObject
{
    Q_OBJECT
public:
    explicit QuestionSql(QString fileName,QObject *parent = nullptr);
    void add_category(int id,QString name,int parent_id = 0);
    void del_category(int id);

    void add_question(int id,int categoryId);
    void del_question(int id);

    void set_goodTime(int id,QTime goodTime);

    int count_total_questions(int categoryId);

    QSqlDatabase getDb() const;

    QString read_questionHTML(int id);
    void write_questionHTML(int id, QString html);
    void set_question_categoryId(int id, int newCategoryId);

    QJsonArray read_answerJSON(int id);
    void write_answerJSON(int id, QJsonArray jsonArray);
    int get_goodTime(int id);
    QString get_category_condString(int categoryId);
    QString get_toLearn_condString(int categoryId);
    QString get_toLearn_condString(QString currentFilter);

    QVariant get_data(int id, QString name);
    void update_question_state(int id, QTime myTime);
    int count_total_questions_to_learn(int categoryId);

    void add_tag(int id, QString name);
    void del_tag(int id);

    template<typename T>void set_data(int id,QString name,T data)
    {
        QSqlQuery query(db);
        QString queryString = QString("UPDATE questions SET %1 = ? WHERE id = %2").arg(name).arg(id);
        query.prepare(queryString);
        query.bindValue(0,data);
        query.exec();
    }

    template<typename T>void set_category_data(int categoryId,QString name,T data)
    {
        QSqlQuery query(db);
        QString queryString = QString("UPDATE categories SET %1 = ? WHERE id = %2").arg(name).arg(categoryId);
        query.prepare(queryString);
        query.bindValue(0,data);
        query.exec();
    }


private:
    QSqlDatabase db;
    QString dbFileName;
    int tempId;
    void _del_category(int id, int parentId);
signals:
};

#endif // QUESTIONSQL_H
