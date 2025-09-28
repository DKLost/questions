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



void ToolFunctions::select_current_underline_text(QTextCursor *cursor) //选择当前光标所在下划线文本
{
    //向左移动到下划线文本的最左侧
    bool flg = false;
    while(cursor->charFormat().fontUnderline())
    {
        if(cursor->atBlockStart() ||
            !cursor->movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 1))
        {
            flg = true;
            break;
        }
    }
    if(!flg)
        cursor->movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);

    //向右选择到下划线文本的最右侧
    flg = false;
    while(cursor->charFormat().fontUnderline())
    {
        if(cursor->atBlockEnd() ||
            !cursor->movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1))
        {
            flg = true;
            break;
        }
    }
    if(!flg)
        cursor->movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 1);
}


