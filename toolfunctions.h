#ifndef TOOLFUNCTIONS_H
#define TOOLFUNCTIONS_H
#include <QString>
#include <QStringList>
#include <QTime>

class ToolFunctions
{
public:
    ToolFunctions();
    static QString sec2string(int seconds);
    static QString timeDurationText(QTime startTime,QTime endTime);
};

#endif // TOOLFUNCTIONS_H
