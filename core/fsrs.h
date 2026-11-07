#ifndef FSRS_H
#define FSRS_H

#include <QList>
#include <QMap>
#include <QTime>

class FSRS
{
public:
    FSRS() = delete;
    static const double defaultW[21];
    static double w[21];
    static double requestRetention;
    static int maximumInterval;
    static double DECAY;
    static double FACTOR;
    // 用时反推评级与新卡估时的可调系数（设置对话框写入，默认值保持既有行为）
    static double easyFactor;        // 用时 < 良好用时 × easyFactor → easy（默认 0.5）
    static double hardFactor;        // 用时 > 良好用时 × hardFactor → hard（默认 1.0）
    static double newCardTimeFactor; // 新卡首次作答用时 × 该系数 = 初始良好用时（默认 2.0）
    static QMap<QString, int> rating;
    static QMap<QString, int> state;

    static void setWeights(const QList<double> &weights); // 写入 21 个权重并重算 DECAY/FACTOR
    static void resetWeights();                           // 恢复内置默认权重

    static double constrain_difficulty(double difficulty);
    static double init_difficulty(int rating);
    static double init_stability(int rating);
    static double forgetting_curve(int elpased_days, double stability);
    static double linear_damping(double deltaD, double oldD);
    static double mean_reversion(double init, double current);
    static double next_difficulty(double d, int rating);
    static double next_recall_stability(double d, double s, double r, int rating);
    static double next_forget_stability(double d, double s, double r);
    static double next_short_term_stability(double s, int rating);
    static int next_interval(double stability);

    static int next_state(QString rating, int elapsedDays, QString &state, double &d, double &s);
    static QString time2rating(QTime myTime, QTime goodTime);
};

#endif // FSRS_H
