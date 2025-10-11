#ifndef TOOLFUNCTIONS_H
#define TOOLFUNCTIONS_H
#include <QString>
#include <QStringList>
#include <QTime>
#include <QApplication>
#include <QLineEdit>
#include <QProcess>
#include <QFile>

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
    static void compile_typst(const QString& inputFile, const QString& outputFile);
    static void write_typst(const QString& typstString, const QString& filePath);
    static void watch_typst_start(QProcess &process, const QString &inputFile, const QString &outputFile);
    static void watch_typst_stop(QProcess &process);
};

#endif // TOOLFUNCTIONS_H
