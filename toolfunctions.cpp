
#include "toolfunctions.h"

ToolFunctions::ToolFunctions() {}

QString ToolFunctions::sec2string(int seconds)
{
    int s = seconds % 60;
    int m = seconds / 60 % 60;

    QTime time;
    time.setHMS(0,m,s,0);
    QString result = time.toString("mm:ss.zzz").chopped(1);
    return result;
}

QString ToolFunctions::timeDurationText(QTime startTime,QTime endTime)
{
    int d = (endTime.hour()*60 + endTime.minute()) - (startTime.hour()*60 + startTime.minute());
    QString text = QString("%1-%2(%3m)")
                       .arg(startTime.toString("HH:mm"))
                       .arg(endTime.toString("HH:mm"))
                       .arg(d);
    return text;
}

