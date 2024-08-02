#ifndef FSRS_H
#define FSRS_H

#include <QObject>
#include <QMap>
#include <QTime>

class FSRS : public QObject
{
    Q_OBJECT
public:
    explicit FSRS(QObject *parent = nullptr);
    static double w[20];
    static double requestRetention;
    static int maximumInterval;
    static double DECAY;
    static double FACTOR;
    static QMap<QString,int> rating;

    static double constrain_difficulty(double difficulty);
    static double init_difficulty(int rating);
    static double next_difficulty(double d,int rating);
    static double next_recall_stability(double d, double s, double r, int rating);
    static double init_stability(int rating);
    static double forgetting_curve(int elpased_days, double stability);
    static double next_forget_stability(double d, double s, double r);
    static double next_short_term_stability(double s, int rating);
    static int next_interval(double stability);
    static double convert_s();

    static QString time2rating(QTime myTime,QTime goodTime);
private:

signals:

};

#endif // FSRS_H
