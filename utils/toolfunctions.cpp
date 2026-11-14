#include "toolfunctions.h"

#include <QPainter>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QUrl>
#include <QCryptographicHash>

namespace {
const QString kTypstCompileCommand = QStringLiteral("compile");
const QString kTypstWatchCommand = QStringLiteral("watch");
const QString kStoredTimeFormat = QStringLiteral("mm'm':ss's'"); // 库内 goodTime 格式
const QString kDisplayTimeFormat = QStringLiteral("mm:ss.zzz");  // 作答计时显示
const QString kShortDisplayTimeFormat = QStringLiteral("mm:ss.zz");
const QString kSessionTimeFormat = QStringLiteral("hh:mm:ss.zzz"); // 复习总计时显示
const char kLineEditClassName[] = "QLineEdit"; // QWidget::inherits 需要 const char*

// DerivedAbs 名语法记号与图片内联参数
const QString kChainArrow = QStringLiteral("→");
const QString kChainTailDelimiter = QStringLiteral(":[");
const QChar kChainArrowChar(0x2192); // →
const QString kZeroWidthSpace = QStringLiteral("\u200B");
constexpr int kImagePadding = 4; // 边框图四周留白

constexpr int kNoRow = -1;

// 填空文本形态：三个空格 + 数字 + 三个空格（编号空）、六个空格（无编号空）
const QRegularExpression kNumberedBlankPattern(QStringLiteral("^\\s{3}[0-9]+\\s{3}$"));
const QRegularExpression kNumberPattern(QStringLiteral("[0-9]+"));
const QRegularExpression kUnnumberedBlankPattern(QStringLiteral("^\\s{3}\\s{3}$"));
constexpr int kNoBlankAtCursor = -1;
constexpr int kUnnumberedBlankNumber = 0;

// 把光标移到下一个下划线填空片段的起点；到文末返回 false
bool moveToNextUnderlineRun(QTextCursor &cursor)
{
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1); // "定位过的空"后光标带选区，这一步会原地失败（不移动也不报错），结果须忽略，由下一次判断是否到文末
    if(!cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1))
        return false;
    while(!cursor.charFormat().fontUnderline())
        if(!cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1))
            return false;
    return true;
}
}

namespace {
// typst 文档前缀模板：%1 = 字号(pt)。整段与原先的 9pt 版本逐字一致，只是把字号提出来供缓存层按视图字号编译
const char *const kTypstPreambleTemplate = R"(
#import "@preview/typsium:0.3.1":ce
#set page(height: auto,width: auto,margin: 0.5pt);
#set text(
    size: %1pt
);
#show math.equation: set text(
    size: %1pt,
    font: (
        "New Computer Modern Math",
        "新宋体"
    )
);
#let revcases(..arr, spacing: 6pt) = {
    $ lr(#stack(dir: ttb, spacing: spacing, ..arr.pos().map(item => align(left, $ #item $))) }) $
};
#show "∥" : "//";
#show math.sum : math.limits
)";
} // namespace

QString ToolFunctions::typstPreamble(int pointSize){
    return QString(kTypstPreambleTemplate).arg(pointSize);
}

QString ToolFunctions::typstMathPrefix = ToolFunctions::typstPreamble(kTypstPointSize); // 与正文字号一致，避免公式比周围文字小
QString ToolFunctions::typstCommand = QStringLiteral("typst");

void ToolFunctions::setTypstCommand(const QString &command)
{
    const QString trimmed = command.trimmed();
    typstCommand = trimmed.isEmpty() ? QStringLiteral("typst") : trimmed;
}

QString ToolFunctions::ms2msz(QString timeString)
{
    return QTime::fromString(timeString, kStoredTimeFormat).toString(kDisplayTimeFormat);
}

QTime ToolFunctions::ms2QTime(QString timeString)
{
    return QTime::fromString(timeString, kStoredTimeFormat);
}

QTime ToolFunctions::msz2QTime(QString timeString)
{
    return QTime::fromString(timeString, kShortDisplayTimeFormat);
}

QLineEdit *ToolFunctions::get_active_LineEdit()
{
    QWidget *focusedWidget = QApplication::focusWidget();
    if(focusedWidget && focusedWidget->inherits(kLineEditClassName))
        return qobject_cast<QLineEdit *>(focusedWidget);
    return nullptr;
}

void ToolFunctions::select_current_underline_text(QTextCursor *cursor) // 选中光标所在的下划线填空片段
{
    bool reachedEdge = false; // 先向左扩展到片段最左端
    while(cursor->charFormat().fontUnderline())
    {
        if(cursor->atBlockStart() || !cursor->movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 1))
        {
            reachedEdge = true;
            break;
        }
    }
    if(!reachedEdge)
        cursor->movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);

    reachedEdge = false; // 再向右扩展到片段最右端
    while(cursor->charFormat().fontUnderline())
    {
        if(cursor->atBlockEnd() || !cursor->movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1))
        {
            reachedEdge = true;
            break;
        }
    }
    if(!reachedEdge)
        cursor->movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 1);
}

bool ToolFunctions::compile_typst(const QString &inputFile, const QString &outputFile)
{
    QProcess process;
    process.start(typstCommand, {kTypstCompileCommand, QStringLiteral("--format"), QStringLiteral("png"),
                                 QStringLiteral("--ppi"), QString::number(typstRenderPpi()),
                                 inputFile, outputFile});
    if(!process.waitForFinished()) // 阻塞等待编译进程结束
    {
        qDebug() << "typst 进程执行超时或出错";
        return false;
    }
    if(process.exitCode() != 0)
    {
        qDebug() << "typst 编译失败：" << process.readAllStandardError();
        return false;
    }
    return true;
}

void ToolFunctions::write_file(const QString &filePath, const QString &text)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadWrite))
    {
        qDebug() << "cannot open file: " << filePath;
        return;
    }
    file.resize(0);
    QTextStream fileIO(&file);
    fileIO << text;
    file.close();
}

void ToolFunctions::watch_typst_start(QProcess &process, const QString &inputFile, const QString &outputFile)
{
    process.start(typstCommand, {kTypstWatchCommand, QStringLiteral("--format"), QStringLiteral("png"),
                                 QStringLiteral("--ppi"), QString::number(typstRenderPpi()),
                                 inputFile, outputFile});
}

int ToolFunctions::typstRenderPpi()
{
    return kTypstSaveBasePpi * kTypstSaveSupersample; // 288：96 逻辑 PPI 的 3 倍像素
}

int ToolFunctions::typstImageScale(const QString &imagePath)
{
    if(imagePath.isEmpty())
        return 1;
    QFile marker(imagePath + QStringLiteral(".2x")); // 沿用旧文件名，内容为 "倍数[:字号]"
    if(!marker.open(QIODevice::ReadOnly | QIODevice::Text))
        return 1;
    const QString text = QString::fromUtf8(marker.readAll()).trimmed();
    if(text.isEmpty())
        return 2; // 旧版空标记：2 倍渲染
    bool ok = false;
    const int scale = text.section(QLatin1Char(':'), 0, 0).toInt(&ok);
    return ok && scale > 1 ? scale : 2; // 兼容旧版写入的异常标记
}

int ToolFunctions::typstImagePointSize(const QString &imagePath)
{
    if(imagePath.isEmpty())
        return 0;
    QFile marker(imagePath + QStringLiteral(".2x"));
    if(!marker.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;
    const QStringList parts = QString::fromUtf8(marker.readAll()).trimmed().split(QLatin1Char(':'));
    if(parts.size() < 2)
        return 0; // 旧标记只记倍数，字号未知
    bool ok = false;
    const int pointSize = parts.at(1).toInt(&ok);
    return ok ? pointSize : 0;
}

void ToolFunctions::markTypstImageScale(const QString &imagePath)
{
    if(imagePath.isEmpty())
        return;
    QFile marker(imagePath + QStringLiteral(".2x"));
    if(marker.open(QIODevice::WriteOnly | QIODevice::Truncate))
        marker.write(QByteArray::number(kTypstSaveSupersample) + ':' + QByteArray::number(kTypstPointSize));
}

void ToolFunctions::installScaledTypstImageResources(QTextDocument *document, const QString &html, qreal dpr)
{
    if(!document || html.isEmpty())
        return;
    const qreal scaleFactor = qMax<qreal>(1.0, dpr);
    const QRegularExpression imgRe(QStringLiteral(R"re(<img\b[^>]*\bsrc="([^"]+)"[^>]*>)re"));
    const QRegularExpression widthRe(QStringLiteral(R"re(\bwidth="(\d+)")re"));
    const QRegularExpression heightRe(QStringLiteral(R"re(\bheight="(\d+)")re"));
    auto matches = imgRe.globalMatch(html);
    while(matches.hasNext())
    {
        const QRegularExpressionMatch match = matches.next();
        const QString tag = match.captured(0);
        const QString path = match.captured(1);
        if(!path.endsWith(QStringLiteral(".png")))
            continue;

        QString sourcePath = path;
        sourcePath.replace(QRegularExpression(QStringLiteral(R"re(\.png$)re")),
                           QStringLiteral(".typmath"));
        if(typstImageScale(path) <= 1 && !QFile::exists(sourcePath))
            continue; // 只处理 typst 图；手工图片保持原样

        const QRegularExpressionMatch widthMatch = widthRe.match(tag);
        const QRegularExpressionMatch heightMatch = heightRe.match(tag);
        if(!widthMatch.hasMatch() || !heightMatch.hasMatch())
            continue;
        const int logicalWidth = widthMatch.captured(1).toInt();
        const int logicalHeight = heightMatch.captured(1).toInt();
        if(logicalWidth <= 0 || logicalHeight <= 0)
            continue;

        const QImage source(path);
        if(source.isNull())
            continue;
        const QSize target(qMax(1, qRound(logicalWidth * scaleFactor)),
                           qMax(1, qRound(logicalHeight * scaleFactor)));
        const QImage scaled = source.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        document->addResource(QTextDocument::ImageResource, QUrl(path), scaled);
    }
}

QSize ToolFunctions::typstDisplaySize(const QString &imagePath)
{
    const QImage image(imagePath);
    if(image.isNull())
        return {};
    const int scale = typstImageScale(imagePath);
    // 源分辨率：新版 96×倍数；旧版 72×倍数。统一折算到 96 逻辑 PPI。
    const int sourcePpi = (scale >= kTypstSaveSupersample ? kTypstSaveBasePpi : 72) * scale;
    // 源字号：标记里有就用；旧图统一按 9pt。统一折算到当前正文字号（12pt）。
    const int storedPointSize = typstImagePointSize(imagePath);
    const int sourcePointSize = storedPointSize > 0 ? storedPointSize : 9;
    const qreal factor = qreal(kTypstBasePpi) / sourcePpi * qreal(kTypstPointSize) / sourcePointSize;
    return QSize(qMax(1, qRound(image.width() * factor)),
                 qMax(1, qRound(image.height() * factor)));
}

QString ToolFunctions::normalizedTypstImageSizes(const QString &html)
{
    const QRegularExpression imgRe(QStringLiteral(R"re(<img\b[^>]*\bsrc="([^"]+)"[^>]*>)re"));
    const QRegularExpression widthRe(QStringLiteral(R"re(\s+width="[^"]*")re"));
    const QRegularExpression heightRe(QStringLiteral(R"re(\s+height="[^"]*")re"));
    const QRegularExpression imgStart(QStringLiteral(R"re(^<img\b)re"));

    QString rebuilt;
    int last = 0;
    auto matches = imgRe.globalMatch(html);
    while(matches.hasNext())
    {
        const QRegularExpressionMatch match = matches.next();
        rebuilt += html.mid(last, match.capturedStart() - last);
        last = match.capturedEnd();

        QString tag = match.captured(0);
        const QString path = match.captured(1);
        if(path.endsWith(QStringLiteral(".png")))
        {
            QString sourcePath = path;
            sourcePath.replace(QRegularExpression(QStringLiteral(R"re(\.png$)re")),
                               QStringLiteral(".typmath"));
            if(typstImageScale(path) > 1 || QFile::exists(sourcePath))
            {
                const QSize size = typstDisplaySize(path);
                if(size.isValid())
                {
                    tag.remove(widthRe);
                    tag.remove(heightRe);
                    tag.replace(imgStart, QStringLiteral("<img width=\"%1\" height=\"%2\"")
                                               .arg(size.width()).arg(size.height()));
                }
            }
        }
        rebuilt += tag;
    }
    rebuilt += html.mid(last);
    return rebuilt;
}

void ToolFunctions::watch_typst_stop(QProcess &process)
{
    if(process.state() == QProcess::Running)
        process.kill();
}

int ToolFunctions::get_cursor_number(QTextCursor *cursor)
{
    select_current_underline_text(cursor);
    const QString selectedText = cursor->selectedText();
    if(kNumberedBlankPattern.match(selectedText).hasMatch()) // 三个空格 + 编号 + 三个空格
        return kNumberPattern.match(selectedText).captured().toInt();
    if(kUnnumberedBlankPattern.match(selectedText).hasMatch()) // 六个空格：未编号的空白填空
        return kUnnumberedBlankNumber;
    return kNoBlankAtCursor;
}

QList<QTextCursor> ToolFunctions::find_blanks_by_number(int number, QTextCursor &fromCursor)
{
    QList<QTextCursor> matches;
    QTextCursor cursor = fromCursor;
    cursor.movePosition(QTextCursor::Start);
    while(moveToNextUnderlineRun(cursor))
        if(get_cursor_number(&cursor) == number)
            matches.append(cursor);
    return matches;
}

QTextCursor ToolFunctions::find_blank_by_number(int number, QTextCursor &fromCursor)
{
    const QList<QTextCursor> matches = find_blanks_by_number(number, fromCursor);
    return matches.isEmpty() ? fromCursor : matches.first();
}

QString ToolFunctions::answerTimeText(const QTime &time)
{
    return time.toString(kDisplayTimeFormat).chopped(1);
}

QString ToolFunctions::sessionTimeText(const QTime &time)
{
    return time.toString(kSessionTimeFormat).chopped(1);
}

bool ToolFunctions::isUnstudied(const QString &answerTimeText)
{
    return msz2QTime(answerTimeText).msecsSinceStartOfDay() == 0;
}

void ToolFunctions::fetchAllRows(QSqlQueryModel *model)
{
    if(!model)
        return;
    while(model->canFetchMore())
        model->fetchMore();
}

// 按显示文本全等查找（避免 SQLite 驱动把 INTEGER 列返回成字符串导致类型不等）
int ToolFunctions::rowOfValue(const QAbstractItemModel *model, const QVariant &value, int column, int startRow)
{
    if(!model)
        return kNoRow;
    const QString wanted = value.toString();
    for(int row = qMax(0, startRow); row < model->rowCount(); ++row)
        if(model->index(row, column).data().toString() == wanted)
            return row;
    return kNoRow;
}

QString ToolFunctions::andFilters(const QString &base, const QString &extra)
{
    if(base.trimmed().isEmpty())
        return extra;
    if(extra.trimmed().isEmpty())
        return base;
    return QStringLiteral("(%1) AND (%2)").arg(base, extra);
}

QString ToolFunctions::idNotInFilter(const QString &idColumn, const QStringList &ids)
{
    if(ids.isEmpty())
        return QString();
    return QStringLiteral("%1 NOT IN (%2)").arg(idColumn, ids.join(QLatin1Char(',')));
}

bool ToolFunctions::isDerivedAbsUnder(const QString &derivedAbs, const QString &ancestor)
{
    return !derivedAbs.isEmpty() && !ancestor.isEmpty() && derivedAbs.startsWith(ancestor + kChainArrow);
}

// DerivedAbs 名按最后一个"顶层"箭头切出 (主词名, 关系段)：末段宾词是限定名（内部也含 →），故按括号配平扫描（与 core 构造口径一致）；被转义的 \[ \] \→ 是名字字面量，不参与计数
bool ToolFunctions::splitDerivedAbsTail(const QString &derivedAbs, QString *subject, QString *relation)
{
    int depth = 0;
    int arrow = -1;
    for(int index = 0; index < derivedAbs.size(); ++index)
    {
        const QChar character = derivedAbs.at(index);
        if(character == QLatin1Char('\\'))
        {
            ++index;
            continue;
        }
        if(character == QLatin1Char('['))
            ++depth;
        else if(character == QLatin1Char(']'))
            --depth;
        else if(depth == 0 && character == kChainArrowChar)
            arrow = index;
    }
    if(arrow < 0)
        return false;
    const QString tail = derivedAbs.mid(arrow + 1);
    const int colon = tail.indexOf(kChainTailDelimiter);
    if(colon < 0)
        return false;
    if(subject)
        *subject = derivedAbs.left(arrow);
    if(relation)
        *relation = tail.left(colon);
    return true;
}

// 表格内插入图片前调用：块内居中并插入零宽字符，避免空块在表格里塌陷
void ToolFunctions::centerCurrentBlockInTable(QTextCursor &cursor)
{
    if(!cursor.currentTable())
        return;
    QTextBlockFormat blockFormat = cursor.blockFormat();
    blockFormat.setAlignment(Qt::AlignCenter);
    cursor.setBlockFormat(blockFormat);
    cursor.insertText(kZeroWidthSpace);
}

// 限宽等比缩放后加边框（边框画在图像边缘外侧）
QImage ToolFunctions::scaledWithBorder(const QImage &image, int maxWidthPx, int borderWidthPx, const QColor &borderColor)
{
    if(image.isNull())
        return image;
    const QImage scaled = image.width() > maxWidthPx
                              ? image.scaledToWidth(maxWidthPx, Qt::SmoothTransformation)
                              : image;
    QImage bordered(scaled.width() + kImagePadding * 2, scaled.height() + kImagePadding * 2, QImage::Format_ARGB32);
    bordered.fill(Qt::white);
    QPainter painter(&bordered);
    painter.drawImage(kImagePadding, kImagePadding, scaled);
    painter.setPen(QPen(borderColor, borderWidthPx));
    const int borderStart = qMax(0, kImagePadding - borderWidthPx);
    painter.drawRect(borderStart, borderStart,
                     bordered.width() - kImagePadding, bordered.height() - kImagePadding);
    painter.end();
    return bordered;
}

QString ToolFunctions::shortHash(const QString &text, int hexLength)
{
    const QString hex = QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
    return hexLength > 0 ? hex.left(hexLength) : hex;
}