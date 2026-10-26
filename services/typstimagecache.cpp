#include "typstimagecache.h"

#include "utils/toolfunctions.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFontMetrics>
#include <QPixmapCache>
#include <QProcess>
#include <QTextStream>
#include <QWidget>

namespace {
constexpr int kKeyHexLength = 32;   // 键取 sha256 前 32 位十六进制，够用且文件名短
constexpr double kLogicalDpi = 96.0; // Qt 逻辑像素口径：字号 pt→逻辑 px 用 96dpi
constexpr int kSupersample = 2;      // 编译超采样倍数（缩小绘制，保证清晰）
const QString kTempDirName = QStringLiteral(".tmp");
} // namespace

TypstImageCache *TypstImageCache::instance()
{
    static TypstImageCache cache;
    return &cache;
}

// 键覆盖一切影响像素的输入：片段源 + 字号 + ppi（前缀由字号决定，含在源里）
QString TypstImageCache::keyOf(const QString &body, int pointSize, int ppi)
{
    const QByteArray raw = body.toUtf8() + '\x1f' + QByteArray::number(pointSize) + '\x1f' + QByteArray::number(ppi);
    return QString::fromLatin1(QCryptographicHash::hash(raw, QCryptographicHash::Sha256).toHex().left(kKeyHexLength));
}

qreal TypstImageCache::pixelsPerPoint(int ppi)
{
    return ppi > 0 ? ppi / 72.0 : 1.0;
}

// 编译字号随控件字体（点值）；字体只给了像素值时按 96dpi 折算；夹在 6..32pt 防止极端字号把图做得过大/过小
int TypstImageCache::pointSizeOf(const QFont &font)
{
    const qreal points = font.pointSizeF() > 0 ? font.pointSizeF()
                                               : QFontMetrics(font).height() * 72.0 / kLogicalDpi;
    return qBound(6, qRound(points), 32);
}

// 编译分辨率 = 96dpi × 屏幕缩放 × 超采样：屏幕像素与图片像素 1:1 落地，不糊
int TypstImageCache::ppiOf(const QWidget *widget)
{
    const qreal dpr = widget ? widget->devicePixelRatioF() : 1.0;
    return qRound(kLogicalDpi * dpr * kSupersample);
}

QSize TypstImageCache::displaySizeOf(const QPixmap &image, int ppi)
{
    const qreal factor = ppi > 0 ? kLogicalDpi / ppi : 1.0;
    return QSize(qMax(1, qRound(image.width() * factor)), qMax(1, qRound(image.height() * factor)));
}

TypstImageCache::TypstImageCache(QObject *parent)
    : QObject(parent)
{
    batchTimer.setSingleShot(true);
    connect(&batchTimer, &QTimer::timeout, this, &TypstImageCache::flush);
}

TypstImageCache::~TypstImageCache()
{
    if(process)
    {
        process->kill();
        process->waitForFinished(1000);
    }
}

QString TypstImageCache::cacheDir() const
{
    // 与 ./data 同口径放在运行目录旁：便于整体清理，且与 typst.exe 的位置一致
    return cachedDir.isEmpty() ? QCoreApplication::applicationDirPath() + QStringLiteral("/.typst-image-cache")
                               : cachedDir;
}

void TypstImageCache::setCacheDir(const QString &dir)
{
    if(dir.isEmpty())
        return;
    cachedDir = QDir::cleanPath(dir);
    QDir().mkpath(cachedDir);
    QPixmapCache::clear(); // 换目录即换数据集：内存里按键缓存的图作废
}

// 换命令即重新尝试：清掉 disabled 与失败记录（同一份缓存目录可换用另一个 typst 二进制）
void TypstImageCache::setCommand(const QString &program)
{
    if(program.isEmpty() || program == this->program)
        return;
    this->program = program;
    disabled = false;
    failed.clear();
}

void TypstImageCache::setBatchDelayMs(int ms)
{
    batchDelayMs = qMax(0, ms);
}

bool TypstImageCache::isDisabled() const
{
    return disabled;
}

int TypstImageCache::compileRunCount() const
{
    return runCount;
}

int TypstImageCache::pendingCount() const
{
    return order.size();
}

QPixmap TypstImageCache::pixmap(const QString &body, int pointSize, int ppi)
{
    if(body.isEmpty() || pointSize <= 0 || ppi <= 0)
        return {};

    const QString key = keyOf(body, pointSize, ppi);
    QPixmap cached;
    if(QPixmapCache::find(key, &cached))
        return cached;

    const QString path = cacheDir() + QLatin1Char('/') + key + QStringLiteral(".png");
    if(QFile::exists(path))
    {
        QPixmap pm(path);
        if(pm.isNull()) // 落盘文件损坏：记失败，避免每次重画都读它
        {
            failed.insert(key);
            qWarning() << "typst 图片缓存：文件无法读取，已跳过" << path;
            return {};
        }
        QPixmapCache::insert(key, pm);
        return pm;
    }

    enqueue(key, Task{body, pointSize, ppi});
    return {};
}

bool TypstImageCache::waitForIdle(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while((!order.isEmpty() || process) && timer.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    return order.isEmpty() && process == nullptr;
}

// ————————— 内部：排队与攒批 —————————

void TypstImageCache::enqueue(const QString &key, const Task &task)
{
    if(disabled || failed.contains(key) || tasks.contains(key))
        return; // 不可用 / 已知失败 / 已排队或正在编译：都不再重复请求
    tasks.insert(key, task);
    order.append(key);
    if(!process && !batchTimer.isActive())
        batchTimer.start(batchDelayMs); // 攒一小段窗口，把同一次重画里的多个片段并成一次编译
}

// 取出与本批字号/ppi 相同的待办键（不同字号留到下一轮，保证一批只用一种页面设置）
QStringList TypstImageCache::takeGroupFor(int pointSize, int ppi) const
{
    QStringList keys;
    for(const QString &key : order)
    {
        const auto it = tasks.constFind(key);
        if(it == tasks.constEnd())
            continue; // 已被失败清理掉，忽略
        if(it->pointSize == pointSize && it->ppi == ppi)
            keys.append(key);
    }
    return keys;
}

void TypstImageCache::flush()
{
    if(disabled || process || order.isEmpty())
        return;

    while(!order.isEmpty() && !tasks.contains(order.first()))
        order.removeFirst(); // 丢掉失败清理留下的残键
    if(order.isEmpty())
        return;

    const Task first = tasks.value(order.first());
    const QStringList keys = takeGroupFor(first.pointSize, first.ppi);
    if(keys.isEmpty())
    {
        order.removeFirst();
        QTimer::singleShot(0, this, &TypstImageCache::flush);
        return;
    }
    startGroup(keys, first.pointSize, first.ppi);
}

void TypstImageCache::startGroup(const QStringList &keys, int pointSize, int ppi)
{
    groupKeys = keys;
    groupDir = cacheDir() + QLatin1Char('/') + kTempDirName + QStringLiteral("-") + QString::number(runCount);
    QDir().mkpath(groupDir);

    const QString inputPath = groupDir + QStringLiteral("/in.typ");
    QFile input(inputPath);
    if(!input.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        finishGroup(false, QStringLiteral("临时文件不可写"));
        return;
    }
    {
        QTextStream out(&input);
        out.setEncoding(QStringConverter::Utf8);
        const QString preamble = ToolFunctions::typstPreamble(pointSize);
        for(int i = 0; i < keys.size(); ++i)
        {
            if(i > 0)
                out << "\n#pagebreak()\n";
            out << preamble << '\n' << tasks.value(keys.at(i)).body << '\n';
        }
    }
    input.close();

    process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this](int, QProcess::ExitStatus) { onProcessFinished(); });
    connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if(error == QProcess::FailedToStart)
            onProcessStartFailed();
    });

    ++runCount;
    process->start(program, {QStringLiteral("compile"), QStringLiteral("--format"), QStringLiteral("png"),
                             QStringLiteral("--ppi"), QString::number(ppi),
                             inputPath, groupDir + QStringLiteral("/out-{p}.png")});
}

void TypstImageCache::onProcessFinished()
{
    QProcess *finished = process;
    process = nullptr;
    const QString stderrText = QString::fromUtf8(finished->readAllStandardError()).trimmed();
    const bool ok = finished->exitStatus() == QProcess::NormalExit && finished->exitCode() == 0;
    finished->deleteLater();
    finishGroup(ok, ok ? QStringLiteral("部分页面未产出") : stderrText.section(QLatin1Char('\n'), 0, 0));
}

void TypstImageCache::onProcessStartFailed()
{
    QProcess *failedProcess = process;
    process = nullptr;
    if(failedProcess)
        failedProcess->deleteLater();
    qWarning() << "typst 不可用（公式/片段回退为纯文本）：" << program;
    disabled = true; // 起不来就不再尝试，调用方一律走纯文本分支
    finishGroup(false, QStringLiteral("typst 未启动"));
}

// 一批收尾：搬页 → 记失败 → 清任务与临时目录 → 通知重画 → 接着下一批
void TypstImageCache::finishGroup(bool ok, const QString &why)
{
    int produced = 0;
    QStringList missing;
    if(ok)
    {
        // {p} 的页序与本批 keys 一一对应：逐页搬到 <key>.png（同卷 rename，落盘即完整）
        for(int i = 0; i < groupKeys.size(); ++i)
        {
            const QString &key = groupKeys.at(i);
            const QString from = groupDir + QStringLiteral("/out-%1.png").arg(i + 1);
            if(!QFile::exists(from))
            {
                missing.append(key);
                continue;
            }
            const QString to = cacheDir() + QLatin1Char('/') + key + QStringLiteral(".png");
            QFile::remove(to);
            if(QFile::rename(from, to))
                ++produced;
            else
                missing.append(key);
        }
    }
    else
    {
        missing = groupKeys;
    }

    if(!missing.isEmpty())
        markFailed(missing, why);
    for(const QString &key : groupKeys)
    {
        tasks.remove(key);
        order.removeAll(key);
    }
    groupKeys.clear();

    if(!groupDir.isEmpty())
    {
        QDir(groupDir).removeRecursively(); // 临时目录随批销毁；缓存目录只留 <key>.png
        groupDir.clear();
    }

    if(produced > 0)
        emit imageReady();
    if(!order.isEmpty())
        QTimer::singleShot(0, this, &TypstImageCache::flush); // 还有别的字号/新增片段：接着下一批
}

void TypstImageCache::markFailed(const QStringList &keys, const QString &why)
{
    for(const QString &key : keys)
    {
        tasks.remove(key);
        order.removeAll(key);
        failed.insert(key); // 本进程内不再重试；下次启动若仍失败会再次记录
    }
    if(!keys.isEmpty())
        qWarning() << "typst 片段编译失败（" << keys.size() << "个）：" << why;
}
