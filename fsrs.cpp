#include "fsrs.h"

double FSRS::w[20] = {0.4197, 1.1869, 3.0412, 15.2441, 7.1434, 0.6477, 1.0007, 0.0674, 1.6597, 0.1712, 1.1178, 2.0225, 0.0904, 0.3025, 2.1214, 0.2498, 2.9466, 0.4891, 0.6468};
double FSRS::requestRetention = 0.9;
int FSRS::maximumInterval = 36500;
double FSRS::DECAY = -0.5;
double FSRS::FACTOR = qPow(0.9,(1 / DECAY)) - 1;
QMap<QString,int> FSRS::rating = {{"wrong",1},{"hard",2},{"good",3},{"easy",4}};

FSRS::FSRS(QObject *parent)
    : QObject{parent}
{
}

double FSRS::constrain_difficulty(double difficulty) {
    return qMin(qMax(difficulty,1.0), 10.0);
}

double FSRS::init_difficulty(int rating) {
    return constrain_difficulty(w[4] - qExp(w[5] * (rating - 1))  + 1);
}

double FSRS::next_difficulty(double d,int rating) {
    double next_d = d - w[6] * (rating - 3) ;
    double mean_reversion = w[7] * w[4] + (1 - w[7]) * next_d;
    return constrain_difficulty(mean_reversion);
}
double FSRS::next_recall_stability(double d,double s,double r,int rating) {
    double hardPenalty = rating == 2 ? w[15] : 1;
    double easyBonus = rating == 4 ? w[16] : 1;
    return s * (1 + qExp(w[8]) * (11 - d) * qPow(s, -w[9]) * (qExp((1 - r) * w[10]) - 1) * hardPenalty * easyBonus);
 }

double FSRS::init_stability(int rating) {
    return qMax(w[rating - 1], 0.1);
}

double FSRS::forgetting_curve(int elpased_days, double stability) {
    return qPow(1 + FACTOR * elpased_days / stability, DECAY);
}

double FSRS::next_forget_stability(double d,double s,double r) {
    return qMin(w[11] * qPow(d, -w[12]) * (qPow(s + 1, w[13]) - 1) * qExp((1 - r) * w[14]), s);
}

double FSRS::next_short_term_stability(double s,int rating) {
    return s * qExp(w[17] * (rating - 3 + w[18]));
}

int FSRS::next_interval(double stability) {
    double new_interval = stability / FACTOR * (qPow(requestRetention, 1 / DECAY) - 1);
    return qMin(qMax(qRound(new_interval), 1), maximumInterval);
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
    else if(myTimeMs < goodTimeMs * 0.4)
    {
        rating = "easy";
    }
    else
    {
        rating = "good";
    }
    return rating;
}

int FSRS::next_state(QString rating,int elapsedDays,QString &state,double &d,double &s)
{
    double nextD = 0.0;
    double nextS = 0.0;
    int interval = 0;
    if(state == "new")
    {
        nextD = init_difficulty(FSRS::rating[rating]);
        nextS = init_stability(FSRS::rating[rating]);
        interval = next_interval(nextS);

        state = "learning";
        if(rating == "easy")
        {
            state = "review";
            double goodS = init_stability(FSRS::rating["good"]);
            int goodInterval = next_interval(goodS);
            interval = qMax(interval,goodInterval + 1);
        }
    }else if(state == "learning")
    {
        nextD = next_difficulty(d,FSRS::rating[rating]);
        nextS = next_short_term_stability(s,FSRS::rating[rating]);
        interval = next_interval(s);

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

        if(rating == "hard")
        {
            double goodS = next_short_term_stability(s,FSRS::rating["good"]);
            int goodInterval = next_interval(goodS);
            interval = qMin(interval,goodInterval);
        }
        if(rating == "good")
        {
            double hardS = next_short_term_stability(s,FSRS::rating["hard"]);
            int hardInterval = next_interval(hardS);
            interval = qMax(interval,hardInterval + 1);
        }
        if(rating == "easy")
        {
            double goodS = next_short_term_stability(s,FSRS::rating["good"]);
            int goodInterval = next_interval(goodS);
            interval = qMax(interval,goodInterval + 1);
        }
    }

    if(rating == "wrong")
    {
        state = "learning";
        interval = 0;
    }

    d = nextD;
    s = nextS;
    interval = qMax(interval - 1,0);

    return interval;
}
