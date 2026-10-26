#ifndef TOOLFUNCTIONS_H
#define TOOLFUNCTIONS_H
#include <QString>
#include <QStringList>
#include <QTime>
#include <QVariant>
#include <QApplication>
#include <QAbstractItemModel>
#include <QSqlQueryModel>
#include <QLineEdit>
#include <QProcess>
#include <QFile>
#include <QRegularExpression>
#include <QList>
#include <QColor>
#include <QImage>
#include <QTextCursor>

// 跨模块通用小工具（全为静态函数，只依赖 Qt、不含项目自定义类型）：时间格式互转、typst 编译/监听、填空定位、模型取全量、SQL 过滤拼装、DerivedAbs 名段边界判断、图片缩放加边框等
class ToolFunctions
{
public:
    // 时间：库内 "mm'm':ss's'" ↔ 界面 "mm:ss.zz" / "hh:mm:ss.zz"
    static QString ms2msz(QString timeString);              // 库内格式 → "mm:ss.zzz"
    static QTime ms2QTime(QString timeString);              // 库内格式 → QTime
    static QTime msz2QTime(QString timeString);             // 界面格式 "mm:ss.zz" → QTime
    static QString answerTimeText(const QTime &time);       // 作答计时文本（mm:ss.zz）
    static QString sessionTimeText(const QTime &time);      // 复习计时文本（hh:mm:ss.zz）
    static bool isUnstudied(const QString &answerTimeText); // 该空是否从未作答（用时为 0）

    // 模型 / SQL
    static void fetchAllRows(QSqlQueryModel *model);        // 取全量（canFetchMore/fetchMore 样板）
    static int rowOfValue(const QAbstractItemModel *model, const QVariant &value, int column = 0, int startRow = 0);
    static QString andFilters(const QString &base, const QString &extra);              // 空串安全的 AND 组合
    static QString idNotInFilter(const QString &idColumn, const QStringList &ids);     // 空列表返回空串

    // DerivedAbs 名（与 core 的构造口径一致）
    static bool isDerivedAbsUnder(const QString &derivedAbs, const QString &ancestor); // derivedAbs 是否为 ancestor 的子树（段边界精确）
    static bool splitDerivedAbsTail(const QString &derivedAbs, QString *subject, QString *relation); // 括号配平切末段：(主词名, 关系段)

    // 编辑器 / 图片
    static void centerCurrentBlockInTable(QTextCursor &cursor); // 表格内插入前：块居中 + 插零宽字符避免行折叠
    static QImage scaledWithBorder(const QImage &image, int maxWidthPx, int borderWidthPx, const QColor &borderColor);
    static QString shortHash(const QString &text, int hexLength = 0); // SHA-256 十六进制前 hexLength 位（0 = 全部）

    static QLineEdit *get_active_LineEdit();
    static void select_current_underline_text(QTextCursor *cursor);
    static void compile_typst(const QString &inputFile, const QString &outputFile);
    static void watch_typst_start(QProcess &process, const QString &inputFile, const QString &outputFile);
    static void watch_typst_stop(QProcess &process);
    static int get_cursor_number(QTextCursor *cursor);      // 光标所在填空编号（-1 无填空、0 无编号填空）
    static QTextCursor find_blank_by_number(int number, QTextCursor &_cursor);
    static QList<QTextCursor> find_blanks_by_number(int number, QTextCursor &_cursor);
    static void write_file(const QString &filePath, const QString &text);

public:
    static QString typstMathPrefix; // typst 数学片段编译前缀（typsium 宏包：字号/数学字体/revcases/∥→// 等）
    static QString typstPreamble(int pointSize); // 同上，但字号可指定（缓存层按视图字号编译片段用）
};

#endif // TOOLFUNCTIONS_H