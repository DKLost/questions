#ifndef STATISTICS_H
#define STATISTICS_H

#include <QString>
#include <QtGlobal>

class QSqlDatabase;

// 工具栏统计快照：待学按当前题目表范围，其余按本地“今天”全局统计
struct StatisticsSnapshot
{
    int dueQuestions = 0;      // 当前范围内：到期且至少有一个空
    int overdueQuestions = 0;  // 其中逾期（nextDate < 今天）
    int dueTodayQuestions = 0; // 其中今天到期
    int allDueQuestions = 0;   // 全局待学（无过滤时与 dueQuestions 相同）

    int reviewedCards = 0;  // 今日复习：复习前状态为 review 的去重空数
    int reviewAttempts = 0; // 今日复习的作答次数

    int learnedCards = 0;  // 今日新学：复习前状态为 new/learning 且答对的去重空数（含失败后重学）
    int learnedAttempts = 0;

    qint64 totalMs = 0;  // 今日学习总用时
    qint64 reviewMs = 0; // 其中复习用时
    qint64 learnMs = 0;  // 其中新学用时
};

// questionFilter：questions 表的当前 WHERE 过滤式（空 = 全表）；用于“待学”的范围口径
StatisticsSnapshot collectStatistics(const QSqlDatabase &db, const QString &questionFilter);

#endif // STATISTICS_H
