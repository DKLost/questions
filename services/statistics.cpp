#include "services/statistics.h"

#include <QDate>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTime>

namespace {
const QString kDateStorageFormat = QStringLiteral("yyyy/MM/dd"); // 与 questions.nextDate 同格式，文本序即日期序
}

StatisticsSnapshot collectStatistics(const QSqlDatabase &db, const QString &questionFilter)
{
    StatisticsSnapshot snapshot;
    const QDate today = QDate::currentDate();
    const QString todayText = today.toString(kDateStorageFormat);

    // 待学：与 QuestionSql::get_toLearn_condString 同口径（到期 + 至少有一个空）
    {
        QSqlQuery query(db);
        QString sql = QStringLiteral("SELECT COUNT(*),"
                                     " COALESCE(SUM(CASE WHEN nextDate < ? THEN 1 ELSE 0 END), 0)"
                                     " FROM questions"
                                     " WHERE nextDate <= ? AND COALESCE(avg10Rating, '') <> ''");
        if(!questionFilter.isEmpty())
            sql += QStringLiteral(" AND (") + questionFilter + QLatin1Char(')');
        query.prepare(sql);
        query.addBindValue(todayText);
        query.addBindValue(todayText);
        if(query.exec() && query.next())
        {
            snapshot.dueQuestions = query.value(0).toInt();
            snapshot.overdueQuestions = query.value(1).toInt();
            snapshot.dueTodayQuestions = snapshot.dueQuestions - snapshot.overdueQuestions;
        }
    }

    // 全局待学：有过滤时用于 tooltip 对照
    if(questionFilter.isEmpty())
    {
        snapshot.allDueQuestions = snapshot.dueQuestions;
    } else
    {
        QSqlQuery query(db);
        query.prepare("SELECT COUNT(*) FROM questions"
                      " WHERE nextDate <= ? AND COALESCE(avg10Rating, '') <> ''");
        query.addBindValue(todayText);
        if(query.exec() && query.next())
            snapshot.allDueQuestions = query.value(0).toInt();
    }

    // 今日：按复习前状态区分“复习”（review）与“新学”（new/learning）
    // 新学只算答对（rating > 1）；复习失败后转入 learning 再答对，会自然进入新学计数
    {
        const QDateTime dayStart(today, QTime(0, 0));
        const qint64 startMs = dayStart.toMSecsSinceEpoch();
        const qint64 endMs = dayStart.addDays(1).toMSecsSinceEpoch();

        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT"
            " COUNT(DISTINCT CASE WHEN review_state = 2 THEN card_id END),"
            " COALESCE(SUM(CASE WHEN review_state = 2 THEN 1 ELSE 0 END), 0),"
            " COUNT(DISTINCT CASE WHEN review_state IN (0, 1) AND review_rating > 1 THEN card_id END),"
            " COALESCE(SUM(CASE WHEN review_state IN (0, 1) AND review_rating > 1 THEN 1 ELSE 0 END), 0),"
            " COALESCE(SUM(review_duration), 0),"
            " COALESCE(SUM(CASE WHEN review_state = 2 THEN review_duration ELSE 0 END), 0),"
            " COALESCE(SUM(CASE WHEN review_state IN (0, 1) THEN review_duration ELSE 0 END), 0)"
            " FROM review_log WHERE review_time >= ? AND review_time < ?"));
        query.addBindValue(startMs);
        query.addBindValue(endMs);
        if(query.exec() && query.next())
        {
            snapshot.reviewedCards = query.value(0).toInt();
            snapshot.reviewAttempts = query.value(1).toInt();
            snapshot.learnedCards = query.value(2).toInt();
            snapshot.learnedAttempts = query.value(3).toInt();
            snapshot.totalMs = query.value(4).toLongLong();
            snapshot.reviewMs = query.value(5).toLongLong();
            snapshot.learnMs = query.value(6).toLongLong();
        }
    }

    return snapshot;
}
