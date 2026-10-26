#include "questionsql.h"
#include "fsrs.h"
#include "utils/toolfunctions.h"
#include <QFile>

// 诊断开关（默认关）：排查 FSRS 记账错位时逐次追记 clozes 写入到 fsrs_debug.log 供复盘，平常保持关闭
static constexpr bool kFsrsDebugTrace = false;

QMap<QString, int> QuestionSql::headerColumnMap = {{"id", 0}, {"name", 1}, {"state", 2}, {"avg10Rating", 3}, {"goodTime", 4}, {"avg10Time", 5}, {"bestTime", 6}, {"nextDate", 7}, {"lastDate", 8}, {"orderNum", 9}, {"abstractName", 10}};

QuestionSql::QuestionSql(QString fileName, QObject *parent)
    : QObject{parent}
{
    dbFileName = fileName;
    db = QSqlDatabase::addDatabase("QSQLITE", "connection1");
    db.setDatabaseName(dbFileName);
    db.open();

    QSqlQuery query(db);

    // clozes
    query.exec("CREATE TABLE IF NOT EXISTS clozes("
               "id INTEGER PRIMARY KEY,"
               "center INTEGER," // being
               "method INTEGER," // being
               "inject INTEGER," // being
               "result TEXT,"    // data {"type":content} 例如{"being":1221} 1221即being id
               "state TEXT,"     // for fsrs
               "avg10Rating TEXT,"
               "goodTime TEXT,"
               "avg10Time TEXT,"
               "bestTime TEXT,"
               "nextDate TEXT," // for fsrs
               "lastDate TEXT," // for fsrs
               "lastD REAL,"    // for fsrs
               "lastS REAL,"    // for fsrs
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

    // questions
    query.exec("CREATE TABLE IF NOT EXISTS questions("
               "id INTEGER PRIMARY KEY,"
               "name TEXT,"
               "state TEXT,"
               "avg10Rating TEXT,"
               "goodTime TEXT,"
               "avg10Time TEXT,"
               "bestTime TEXT,"
               "nextDate TEXT,"
               "lastDate TEXT,"
               "orderNum INTEGER,"
               "abstractName TEXT)");                            // 挂载的 DerivedAbs 名（与知识索引区行的 DerivedAbsRole 全等）
    // 每个概念至多一份题目；NULL 表示未关联，唯一索引下互不冲突（独立索引比建表 UNIQUE 更便于日后重建）
    query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_questions_abstract"
               " ON questions(abstractName)");
}

// question
void QuestionSql::add_question(int id, QString name, QString abstractName)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO questions(id,name,state,avg10Rating,goodTime,avg10Time,bestTime,nextDate,lastDate,orderNum,abstractName)"
                  " VALUES(?,?,?,?,?,?,?,?,?,?,?)");
    query.bindValue(0, id);                                          // id
    query.bindValue(1, name);                                        // name
    query.bindValue(2, "new");                                       // state
    query.bindValue(3, "");                                          // avg10Rating
    query.bindValue(4, "00m:00s");                                   // goodTime
    query.bindValue(5, "");                                          // avg10Time
    query.bindValue(6, "");                                          // bestTime
    query.bindValue(7, QDate::currentDate().toString("yyyy/MM/dd")); // nextDate
    query.bindValue(8, "");                                          // lastDate
    query.bindValue(9, -1);                                          // orderNum
    // 未选概念时写 NULL（空串会占用唯一索引，导致第二条未关联题目插不进去）
    query.bindValue(10, abstractName.isEmpty() ? QVariant(QMetaType::fromType<QString>())
                                               : QVariant(abstractName));
    query.exec();

    QString dirPath = QString("./data/%1").arg(id);
    QDir dir(".");
    dir.mkpath(dirPath);
    QFile fileQ(dirPath + "/question.html");
    QFile fileA(dirPath + "/answer.json");
    fileQ.open(QIODevice::ReadWrite);
    fileA.open(QIODevice::ReadWrite);

    write_answerJSON(id, QJsonArray{});
    fileQ.close();
    fileA.close();
}
void QuestionSql::del_question(int id)
{
    QSqlQuery query(db);
    const QJsonArray array = read_answerJSON(id);
    for(auto item : array)
    {
        int aId = item.toObject().value("id").toInt();
        del_cloze(aId);
    }
    query.prepare("DELETE FROM questions WHERE id = ?");
    query.bindValue(0, id);
    query.exec();
    QString dirPath = QString("./data/%1").arg(id);
    QDir dir(dirPath);
    dir.removeRecursively();
}

// clozes
void QuestionSql::add_cloze(int id)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO clozes VALUES(:id,:center,:method,:inject,:result,"
                  ":state,:avg10Rating,:goodTime,:avg10Time,:bestTime,:nextDate,:lastDate,:lastD,:lastS,"
                  ":time0,:time1,:time2,:time3,:time4,:time5,:time6,:time7,:time8,:time9,"
                  ":bindCount,:bindQuestions)");
    query.bindValue(":id", id);
    query.bindValue(":center", -1);
    query.bindValue(":method", -1);
    query.bindValue(":inject", -1);
    query.bindValue(":result", "");
    query.bindValue(":state", "new");
    query.bindValue(":avg10Rating", "");
    query.bindValue(":goodTime", "00m:00s");
    query.bindValue(":avg10Time", "");
    query.bindValue(":bestTime", "");
    query.bindValue(":nextDate", QDate::currentDate().toString("yyyy/MM/dd"));
    query.bindValue(":lastDate", "");
    query.bindValue(":lastD", -1);
    query.bindValue(":lastS", -1);
    for(int i = 0; i < 10; i++) // time0-9
        query.bindValue(QString{":time"} + QString::number(i), "");
    query.bindValue(":bindCount", 0);
    query.bindValue(":bindQuestions", "");
    query.exec();
}
void QuestionSql::del_cloze(int id)
{
    QSqlQuery query(db);
    int bindCount = get_value("clozes", id, "bindCount").toInt();
    if(bindCount > 1)
        dec_cloze_bind_count(id);
    else
    {
        query.prepare("DELETE FROM clozes WHERE id = ?");
        query.bindValue(0, id);
        query.exec();
    }
}
void QuestionSql::inc_cloze_bind_count(int aId)
{
    int bindCount = get_value("clozes", aId, "bindCount").toInt();
    set_value("clozes", aId, "bindCount", bindCount + 1);
}
void QuestionSql::dec_cloze_bind_count(int aId)
{
    int bindCount = get_value("clozes", aId, "bindCount").toInt();
    set_value("clozes", aId, "bindCount", bindCount - 1);
}
void QuestionSql::update_cloze_state(int id, QTime myTime)
{
    const QTime goodTime = QTime::fromString(get_value("clozes", id, "goodTime").toString(), "mm'm':ss's'");
    const QString rating = FSRS::time2rating(myTime, goodTime);

    if(goodTime.msecsSinceStartOfDay() == 0)
    {
        if(rating != "wrong")
            set_value("clozes", id, "goodTime",
                      QTime::fromMSecsSinceStartOfDay(myTime.msecsSinceStartOfDay() * 2).toString("mm'm':ss's'"));
        return; // 新卡只估算良好时间，不推进调度（既有不变量）
    }
    writeClozeState(id, myTime, rating, goodTime);
}

// 用户强制评分（manual 回忆卡）：评分取自用户而非用时；新卡只估算良好时间，不排期
void QuestionSql::update_cloze_state_by_rating(int id, QTime myTime, QString rating)
{
    QTime goodTime = QTime::fromString(get_value("clozes", id, "goodTime").toString(), "mm'm':ss's'");
    if(goodTime.msecsSinceStartOfDay() == 0 && rating != "wrong")
    {
        goodTime = QTime::fromMSecsSinceStartOfDay(myTime.msecsSinceStartOfDay() * 2);
        set_value("clozes", id, "goodTime", goodTime.toString("mm'm':ss's'"));
        return;
    }
    writeClozeState(id, myTime, rating, goodTime);
}

// 按给定评分写入排期与用时统计：wrong 只排到当天且不记账；无用时（纯回忆未作答）同样不记时间统计
void QuestionSql::writeClozeState(int id, QTime myTime, QString rating, QTime goodTime)
{
    const QDate lastDate = QDate::fromString(get_value("clozes", id, "lastDate").toString(), "yyyy/MM/dd");
    const int elapsedDays = lastDate.isValid() ? lastDate.daysTo(QDate::currentDate()) : 0;
    QString state = get_value("clozes", id, "state").toString();
    double d = get_value("clozes", id, "lastD").toDouble();
    double s = get_value("clozes", id, "lastS").toDouble();

    if(kFsrsDebugTrace) // 写入前：评级/用时/前值（曾用于定位"答对被判 hard 且未记用时"）
    {
        QFile log(QStringLiteral("fsrs_debug.log"));
        if(log.open(QIODevice::Append | QIODevice::Text))
            log.write(QStringLiteral("id=%1 rating=%2 myTime=%3ms state=%4 d:%5→%6 s:%7→%8\n")
                          .arg(id).arg(rating).arg(myTime.msecsSinceStartOfDay()).arg(state)
                          .arg(d, 0, 'g', 12).arg(0.0, 0, 'g', 12).arg(s, 0, 'g', 12).arg(0.0, 0, 'g', 12)
                          .toUtf8());
    }

    const int interval = FSRS::next_state(rating, elapsedDays, state, d, s);

    if(kFsrsDebugTrace) // 写入后：补记 D/S（next_state 就地改了 d、s）
    {
        QFile log(QStringLiteral("fsrs_debug.log"));
        if(log.open(QIODevice::Append | QIODevice::Text))
            log.write(QStringLiteral("     → d=%1 s=%2 state=%3 interval=%4\n")
                          .arg(d, 0, 'g', 12).arg(s, 0, 'g', 12).arg(state).arg(interval)
                          .toUtf8());
    }
    set_value("clozes", id, "state", state);
    set_value("clozes", id, "lastD", d);
    set_value("clozes", id, "lastS", s);
    set_value("clozes", id, "lastDate", QDate::currentDate().toString("yyyy/MM/dd"));
    set_value("clozes", id, "nextDate", QDate::currentDate().addDays(interval).toString("yyyy/MM/dd"));

    if(rating == "wrong" || myTime.msecsSinceStartOfDay() <= 0)
        return;

    const QString newTime = myTime.toString("mm:ss.zzz").chopped(1);
    const QString oldBestTime = get_value("clozes", id, "bestTime").toString();
    if(oldBestTime == "" || newTime < oldBestTime)
        set_value("clozes", id, "bestTime", newTime);

    int count = 1;
    int tSum = myTime.msecsSinceStartOfDay();
    for(int i = 9; i > 0; i--)
    {
        QString tNameBefore = QString("time%1").arg(i - 1);
        QString tValueBefore = get_value("clozes", id, tNameBefore).toString();
        QString tName = QString("time%1").arg(i);
        QString tValue = get_value("clozes", id, tName).toString();
        set_value("clozes", id, tName, tValueBefore);
        if(tValue == "")
            continue;
        count++;
        tSum += QTime::fromString(tValue, "mm:ss.zz").msecsSinceStartOfDay();
    }
    set_value("clozes", id, "time0", newTime);

    int tAvg = tSum / count;
    QTime newAvg10Time = QTime::fromMSecsSinceStartOfDay(tAvg);
    set_value("clozes", id, "avg10Time", newAvg10Time.toString("mm:ss.zzz").chopped(1));
    set_value("clozes", id, "avg10Rating", FSRS::time2rating(newAvg10Time, goodTime));
}

// other
QVariant QuestionSql::get_value(QString table, int id, QString column)
{
    QSqlQuery query(db);
    QString queryString = QString("SELECT %1 FROM %2 WHERE id = %3").arg(column).arg(table).arg(id);
    query.exec(queryString);
    if(query.next())
    {
        return query.value(0);
    }
    return QVariant{};
}
template <typename T>
void QuestionSql::set_value(QString table, int id, QString column, T value)
{
    QSqlQuery query(db);
    QString queryString = QString("UPDATE %1 SET %2 = ? WHERE id = %3").arg(table).arg(column).arg(id);
    query.prepare(queryString);
    query.bindValue(0, value);
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
void QuestionSql::write_answerJSON(int qId, QJsonArray jsonArray)
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
QString QuestionSql::read_questionMd(int id)
{
    QString filePath = QString("./data/%1/question.md").arg(id);
    QFile file(filePath);
    file.open(QIODevice::ReadOnly);
    QString md = file.readAll();
    file.close();
    return md;
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
void QuestionSql::write_questionHTML(int id, QString html)
{
    QString filePath = QString("./data/%1/question.html").arg(id);
    QFile file(filePath);
    file.open(QIODevice::ReadWrite);
    file.resize(0);
    QTextStream fileIO(&file);
    fileIO << html;
    file.close();
}
void QuestionSql::write_questionMd(int id, QString markdown)
{
    QString filePath = QString("./data/%1/question.md").arg(id);
    QFile file(filePath);
    file.open(QIODevice::ReadWrite);
    file.resize(0);
    QTextStream fileIO(&file);
    fileIO << markdown;
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
        QString aState = get_value("clozes", aId, "state").toString();
        QTime aGoodTime = QTime::fromString(get_value("clozes", aId, "goodTime").toString(), "mm'm':ss's'");
        QTime aAvg10Time = QTime::fromString(get_value("clozes", aId, "avg10Time").toString(), "mm:ss.zz");
        QTime aBestTime = QTime::fromString(get_value("clozes", aId, "bestTime").toString(), "mm:ss.zz");
        QDate aNextDate = QDate::fromString(get_value("clozes", aId, "nextDate").toString(), "yyyy/MM/dd");
        QDate aLastDate = QDate::fromString(get_value("clozes", aId, "lastDate").toString(), "yyyy/MM/dd");

        if(FSRS::state[aState] < FSRS::state[state])
            state = aState;
        goodTime = goodTime.addMSecs(aGoodTime.msecsSinceStartOfDay());
        avg10Time = avg10Time.addMSecs(aAvg10Time.msecsSinceStartOfDay());
        bestTime = bestTime.addMSecs(aBestTime.msecsSinceStartOfDay());
        if(aNextDate < nextDate)
            nextDate = aNextDate;
        if(aLastDate < lastDate)
            lastDate = aLastDate;
    }
    if(answerArray.empty())
    {
        state = "new";
        nextDate = QDate::currentDate();
    } else
    {
        avg10Rating = FSRS::time2rating(avg10Time, goodTime);
    }

    set_value("questions", id, "state", state);
    set_value("questions", id, "avg10Rating", avg10Rating);
    set_value("questions", id, "avg10Time", avg10Time.toString("mm:ss.zzz").chopped(1));
    set_value("questions", id, "goodTime", goodTime.toString("mm'm':ss's'"));
    set_value("questions", id, "bestTime", bestTime.toString("mm:ss.zzz").chopped(1));
    set_value("questions", id, "nextDate", nextDate.toString("yyyy/MM/dd"));
    set_value("questions", id, "lastDate", lastDate.toString("yyyy/MM/dd"));
}

// "待学习"条件：在（可选的）当前显示表过滤之上，再要求 nextDate <= 今天，且该题至少有一个空（cloze）可答
QString QuestionSql::get_toLearn_condString(QString currentFilter)
{
    QString currentDate = QDate::currentDate().toString("yyyy/MM/dd");
    // nextDate 以 yyyy/MM/dd 文本存储，同格式下字典序即日期序，可直接比较
    QString condString = QString("nextDate <= '%1'").arg(currentDate);
    // 无空的题作答区是空的，进"待学习"只会白占一行：answers 存在 answer.json 里，SQL 侧取它的聚合标志判空
    // 用 avg10Rating 而非 goodTime——update_question_state 只在答案集为空时把它留空串（有答案时 time2rating 必给非空评级），
    // 而 goodTime 对"有答案但一次都没答对"的题同样是 00m:00s，一刀切会把新题全部挡在待学习之外
    condString += QStringLiteral(" AND COALESCE(avg10Rating, '') <> ''");
    // 过滤为空时不能再拼 "(空)"——那会产出 "AND ()" 的非法 SQL，使整个查询失败（表空）
    if(!currentFilter.isEmpty())
        condString = QString("(%1) AND (%2)").arg(condString, currentFilter);
    return condString;
}

QTime QuestionSql::get_question_learn_time(int qId)
{
    QTime time = QTime::fromMSecsSinceStartOfDay(0);
    QJsonArray answerArray = read_answerJSON(qId);
    for(auto a : answerArray)
    {
        int aId = a.toObject().value("id").toInt();
        QString aState = get_value("clozes", aId, "state").toString();
        QTime aGoodTime = QTime::fromString(get_value("clozes", aId, "goodTime").toString(), "mm'm':ss's'");
        QDate aNextDate = QDate::fromString(get_value("clozes", aId, "nextDate").toString(), "yyyy/MM/dd");

        if(aNextDate <= QDate::currentDate())
        {
            time = time.addMSecs(aGoodTime.msecsSinceStartOfDay());
        }
    }
    return time;
}

void QuestionSql::resetFSRSData(int clozeId)
{
    set_value("clozes", clozeId, "state", "new");
    set_value("clozes", clozeId, "avg10Rating", "");
    set_value("clozes", clozeId, "avg10Time", "");
    set_value("clozes", clozeId, "goodTime", "00m:00s");
    set_value("clozes", clozeId, "bestTime", "");
    set_value("clozes", clozeId, "nextDate", QDate::currentDate().toString("yyyy/MM/dd"));
    set_value("clozes", clozeId, "lastDate", "");
    set_value("clozes", clozeId, "lastD", -1);
    set_value("clozes", clozeId, "lastS", -1);
    for(int i = 0; i < 10; i++) // time0-9
        set_value("clozes", clozeId, QString{"time"} + QString::number(i), "");

    return;
}
