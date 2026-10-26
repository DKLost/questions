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
    explicit QuestionSql(QString fileName, QObject *parent = nullptr);

    // static
    static QMap<QString, int> headerColumnMap; // 字段名 → questions 模型的列下标（0-based）

    // question
    void add_question(int id, QString name, QString abstractName = QString()); // abstractName：挂载的 DerivedAbs 名，空表示未关联
    void del_question(int id);
    void update_question_state(int id);
    QString get_toLearn_condString(QString currentFilter); // 待学习：到期 且 至少有一个空（用 avg10Rating 非空判，见实现注释）
    QTime get_question_learn_time(int qId);

    // clozes
    void add_cloze(int id);
    void del_cloze(int id);
    void inc_cloze_bind_count(int id);
    void dec_cloze_bind_count(int id);
    void update_cloze_state(int id, QTime myTime);
    // 强制评分（manual 回忆卡）：评分取自用户而非用时；新卡只估算良好时间、不排期
    void update_cloze_state_by_rating(int id, QTime myTime, QString rating);

    // other

    QVariant get_value(QString table, int id, QString column);
    template <typename T>
    void set_value(QString table, int id, QString column, T value);
    QJsonArray read_answerJSON(int id);
    void write_answerJSON(int id, QJsonArray jsonArray);
    QString read_questionHTML(int id);
    QString read_questionMd(int id);
    void write_questionHTML(int id, QString html);
    void write_questionMd(int id, QString markdown);
    QSqlDatabase getDb() const;
    int get_max_id(QString table);


    void resetFSRSData(int clozeId);
private:
    QSqlDatabase db;
    QString dbFileName;
    void writeClozeState(int id, QTime myTime, QString rating, QTime goodTime); // 两个入口共用的排期与记账实现
signals:
};

#endif // QUESTIONSQL_H
