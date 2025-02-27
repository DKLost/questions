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

QString ToolFunctions::ms2msz(QString timeString)
{
    QTime t = QTime::fromString(timeString,"mm'm':ss's'");
    return t.toString("mm:ss.zzz");
}

QTime ToolFunctions::ms2QTime(QString timeString)
{
    return QTime::fromString(timeString,"mm'm':ss's'");
}

QTime ToolFunctions::msz2QTime(QString timeString)
{
    return QTime::fromString(timeString,"mm:ss.zz");
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

QLineEdit* ToolFunctions::get_active_LineEdit() {
    QWidget* focusedWidget = QApplication::focusWidget();
    if (focusedWidget && focusedWidget->inherits("QLineEdit")) {
        return qobject_cast<QLineEdit*>(focusedWidget);
    }
    return nullptr;
}

