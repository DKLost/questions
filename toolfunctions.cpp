#include "toolfunctions.h"

QString ToolFunctions::typstMathPrefix = R"(
#set page(height: auto,width: auto,margin: 0.5pt);
#set text(size: 9pt);
#let revcases(..arr, spacing: 6pt) = {
    $ lr(#stack(dir: ttb, spacing: spacing, ..arr.pos().map(item => align(left, $ #item $))) }) $
};
#show "∥" : "//";
#show math.sum : math.limits
)";

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

void ToolFunctions::compile_typst(const QString &inputFile, const QString &outputFile)
{
    QProcess process;

    // 设置命令和参数
    QStringList arguments;
    arguments << "compile" << inputFile << outputFile;

    // 启动 typst 进程
    process.start("typst", arguments);

    // 等待进程完成
    if (process.waitForFinished()) {
        if (process.exitCode() == 0) {
            qDebug() << "编译成功";
        } else {
            qDebug() << "编译失败：" << process.readAllStandardError();
        }
    } else {
        qDebug() << "进程执行超时或出错";
    }
}

void ToolFunctions::write_typst(const QString &typstString, const QString &filePath)
{
    QFile file(filePath);
    file.open(QIODevice::ReadWrite);
    file.resize(0);
    QTextStream fileIO(&file);
    fileIO << typstString;
    file.close();
}

void ToolFunctions::watch_typst_start(QProcess &process,const QString &inputFile, const QString &outputFile) {
    QStringList arguments;
    arguments << "watch" << inputFile << outputFile;
    process.start("typst", arguments);
}
void ToolFunctions::watch_typst_stop(QProcess &process) {
    if (process.state() == QProcess::Running) {
        process.kill();
    }
}

int ToolFunctions::get_cursor_number(QTextCursor *cursor) //获取当前cursor所在填空的编号9/8
{
    int num = -1;
    ToolFunctions::select_current_underline_text(cursor);
    QString targetText = cursor->selectedText();

    QRegularExpression regex("^\\s{3}[0-9]+\\s{3}$");
    QRegularExpressionMatch match;
    match = regex.match(targetText);
    if(match.hasMatch())
    {
        regex.setPattern("[0-9]+");
        match = regex.match(targetText);
        num = match.capturedTexts()[0].toInt();
    }else
    {
        regex.setPattern("^\\s{3}\\s{3}$");
        match = regex.match(targetText);
        if(match.hasMatch())
            num = 0;
    }
    return num;
}

QTextCursor ToolFunctions::find_blank_by_number(int number,QTextCursor &_cursor)
{
    QTextCursor cursor = _cursor;
    cursor.movePosition(QTextCursor::Start);
    //int number = get_cursor_number(&cursor);
    bool flg = false;
    while(!flg)
    {
        flg = !cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);
        flg = !cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);
        while(!cursor.charFormat().fontUnderline())
        {
            if(!cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1))
            {
                flg = true;
                break;
            }
        }
        if(!flg)
        {
            int nextNumber = get_cursor_number(&cursor);
            if(nextNumber == number)
            {
                return cursor;
            }
        }
    }
    return _cursor;
}
