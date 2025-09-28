#include "core/fsrs.h"
//v6.1.1
double FSRS::w[21] = {0.212, 1.2931, 2.3065, 8.2956, 6.4133, 0.8334, 3.0194, 0.001, 1.8722, 0.1666, 0.796,
                      1.4835, 0.0614, 0.2629, 1.6483, 0.6014, 1.8729, 0.5425, 0.0912, 0.0658, 0.1542};

double FSRS::requestRetention = 0.9;
int FSRS::maximumInterval = 36500;
double FSRS::DECAY = -w[20];
double FSRS::FACTOR = qPow(0.9,(1.0 / DECAY)) - 1.0;
QMap<QString,int> FSRS::rating = {{"wrong",1},{"hard",2},{"good",3},{"easy",4}};
QMap<QString,int> FSRS::state = {{"new",1},{"learning",2},{"review",3}};

FSRS::FSRS(QObject *parent)
    : QObject{parent}
{
}

double FSRS::constrain_difficulty(double difficulty) {
    return qMin(qMax(difficulty,1.0), 10.0);
}
double FSRS::forgetting_curve(int elpased_days, double stability) {
    return qPow(1.0 + FACTOR * elpased_days / stability, DECAY);
}
int FSRS::next_interval(double stability) {
    double newInterval = stability / FACTOR * (qPow(requestRetention, 1.0 / DECAY) - 1.0);
    return qMin(qMax(qRound(newInterval), 1), maximumInterval);
}
double FSRS::linear_damping(double deltaD,double oldD)
{
    return deltaD * (10.0 - oldD) / 9.0;
}
double FSRS::next_difficulty(double d,int rating) {
    double deltaD = -w[6] * (rating - 3) ;
    double nextD = d + linear_damping(deltaD,d);
    return constrain_difficulty(mean_reversion(init_difficulty(rating["easy"]),nextD));
}
double FSRS::mean_reversion(double init,double current) //w[7] * w[4] + (1 - w[7]) * nextD
{
    return w[7] * init + (1 - w[7]) * current;
}
double FSRS::next_recall_stability(double d,double s,double r,int rating) {
    double hardPenalty = rating == rating["hard"] ? w[15] : 1;
    double easyBonus = rating == rating["easy"] ? w[16] : 1;
    return s * (1 + qExp(w[8]) * (11 - d) * qPow(s, -w[9]) * (qExp((1 - r) * w[10]) - 1) * hardPenalty * easyBonus);
}
double FSRS::next_forget_stability(double d,double s,double r) {
    double sMin = s / qExp(w[17] * w[18]);
    return qMin(w[11] * qPow(d, -w[12]) * (qPow(s + 1, w[13]) - 1) * qExp((1 - r) * w[14]), sMin);
}
double FSRS::next_short_term_stability(double s,int rating) {
    double sinc = qExp(w[17] * (rating - 3 + w[18])) * qPow(s,-w[19]);
    if (rating >= rating["good"]) {
        sinc = qMax(sinc, 1.0);
    }
    return s * sinc;

}
double FSRS::init_difficulty(int rating) {
    return constrain_difficulty(w[4] - qExp(w[5] * (rating - 1))  + 1);
}
double FSRS::init_stability(int rating) {
    return qMax(w[rating - 1], 0.1);
}

int FSRS::next_state(QString rating,int elapsedDays,QString &state,double &d,double &s)
{
    double nextD = d;
    double nextS = s;
    int interval = 0;

    if(state == "new")
    {

        if(d == -1 || s == -1)
        {
            nextD = init_difficulty(FSRS::rating[rating]);
            nextS = init_stability(FSRS::rating[rating]);
        }

        if(rating != "wrong")
        {
            interval = next_interval(nextS);
            state = "learning";
            if(rating == "easy")
            {
                state = "review";
                double goodS = init_stability(FSRS::rating["good"]);
                int goodInterval = next_interval(goodS);
                interval = qMax(interval,goodInterval + 1);
            }
        }
    }
    else if(state == "learning")
    {
        nextD = next_difficulty(d,FSRS::rating[rating]);
        nextS = next_short_term_stability(s,FSRS::rating[rating]);
        interval = next_interval(s);

        if(rating == "wrong") interval = 0;
        if(rating == "good") state = "review";
        if(rating == "easy")
        {
            state ="review";
            double goodS = next_short_term_stability(s,FSRS::rating["good"]);
            int goodInterval = next_interval(goodS);
            interval = qMax(interval,goodInterval + 1);
        }
    }else if(state == "review")
    {
        double r = forgetting_curve(elapsedDays,s);
        nextD = next_difficulty(d,FSRS::rating[rating]);
        nextS = next_recall_stability(d,s,r,FSRS::rating[rating]);
        interval = next_interval(nextS);

        if(rating == "wrong")
        {
            nextS = next_forget_stability(d,s,r);
            state = "learning";
            interval = 0;
        }
        if(rating == "hard")
        {
            double goodS = next_recall_stability(d,s,r,FSRS::rating["good"]);
            int goodInterval = next_interval(goodS);
            interval = qMin(interval,goodInterval);
        }
        if(rating == "good")
        {
            double hardS = next_recall_stability(d,s,r,FSRS::rating["hard"]);
            int hardInterval = next_interval(hardS);
            interval = qMax(interval,hardInterval + 1);
        }
        if(rating == "easy")
        {
            double goodS = next_recall_stability(d,s,r,FSRS::rating["good"]);
            int goodInterval = next_interval(goodS);
            interval = qMax(interval,goodInterval + 1);
        }

    }

    d = nextD;
    s = nextS;
    interval = qMax(interval - 1,0);

    return interval;
}

QString FSRS::time2rating(QTime myTime, QTime goodTime)
{
    int goodTimeMs = goodTime.msecsSinceStartOfDay();
    int myTimeMs = myTime.msecsSinceStartOfDay();

    QString rating;
    if(myTime.msecsSinceStartOfDay() == 0)
    {
        rating = "wrong";
        return rating;
    }
    if(myTimeMs > goodTimeMs)
    {
        rating = "hard";
    }
    else if(myTimeMs < goodTimeMs * 0.5)
    {
        rating = "easy";
    }
    else
    {
        rating = "good";
    }
    return rating;
}
