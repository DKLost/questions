#ifndef TOOLFUNCTIONS_H
#define TOOLFUNCTIONS_H
#include <QString>
#include <QStringList>
#include <QTime>
#include <QApplication>
#include <QLineEdit>

class ToolFunctions
{
public:
    ToolFunctions();
    static QString sec2string(int seconds);
    static QString timeDurationText(QTime startTime,QTime endTime);
    static QString ms2msz(QString timeString);
    static QTime ms2QTime(QString timeString);
    static QLineEdit *get_active_LineEdit();
    static QTime msz2QTime(QString timeString);
    static void select_current_underline_text(QTextCursor *cursor);
};

#endif // TOOLFUNCTIONS_H
