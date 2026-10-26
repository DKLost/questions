#ifndef TYPSTIMAGECACHE_H
#define TYPSTIMAGECACHE_H

#include <QFont>
#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTimer>

class QProcess;
class QWidget;

// typst 片段 → PNG 的公共缓存（内容寻址）：
//   · 键 = 片段源 + 字号(pt) + ppi —— 三者任一变化都是不同的图，故改前缀/字号不会误命中旧图；
//   · 落盘后**永不删除**（同内容再次出现直接命中，省掉一次编译），代价只是磁盘；目录可整体清空；
//   · 未命中的请求在合并窗口内攒批，一次 typst 调用按页出一组 PNG（{p} 多页输出），摊掉进程启动成本；
//   · typst 不可用或某片段编译失败时只记失败、不反复重试，调用方（delegate）回退显示纯文本。
class TypstImageCache : public QObject
{
    Q_OBJECT

public:
    static TypstImageCache *instance();

    static QString keyOf(const QString &body, int pointSize, int ppi); // 缓存键（sha256 前缀）
    static qreal pixelsPerPoint(int ppi);                             // 图与字号的换算：1pt 对应多少像素

    // 字号 / 分辨率口径（键的两个维度，必须与编译时一致；delegate 与复习窗标签共用同一套，避免各自漂移）
    static int pointSizeOf(const QFont &font);                 // 控件字体 → 编译字号（pt，夹在 6..32）
    static int ppiOf(const QWidget *widget);                   // 控件 → 编译分辨率（96 × 屏幕缩放 × 超采样）
    static QSize displaySizeOf(const QPixmap &image, int ppi); // 编译图 → 逻辑显示尺寸（缩回 96dpi 口径）

    QString cacheDir() const;                 // 默认 <运行目录>/.typst-image-cache（含 "typst" 命令时的相对口径）
    void setCacheDir(const QString &dir);     // 沙盒/测试可注入
    void setCommand(const QString &program);  // typst 可执行文件；默认 "typst"（换命令会清掉不可用标记，重新尝试）
    void setBatchDelayMs(int ms);             // 攒批窗口（默认 30ms）
    bool isDisabled() const;                  // typst 起不来 → true，此后只回退不重试
    int compileRunCount() const;              // 累计起过几次 typst 进程（验证攒批是否生效）
    int pendingCount() const;

    // 命中 → 同步返回；未命中 → 返回空并把该片段排进队列，编译完成后发 imageReady()
    QPixmap pixmap(const QString &body, int pointSize, int ppi);
    bool waitForIdle(int timeoutMs = 20000); // 测试/预取用：等队列与当前进程收尾（会跑事件循环）

signals:
    void imageReady(); // 有新图落盘：视图收到后重画可见行

private slots:
    void flush();

private:
    explicit TypstImageCache(QObject *parent = nullptr);
    ~TypstImageCache() override;

    struct Task
    {
        QString body;
        int pointSize = 0;
        int ppi = 0;
    };

    void enqueue(const QString &key, const Task &task);
    void startGroup(const QStringList &keys, int pointSize, int ppi);
    void onProcessFinished();
    void onProcessStartFailed();
    void finishGroup(bool ok, const QString &why);       // 一批收尾：搬页/记失败/清临时目录/通知重画
    void markFailed(const QStringList &keys, const QString &why);
    QStringList takeGroupFor(int pointSize, int ppi) const; // 取出与本批同字号/ppi 的待办键（保持入队序）

    QHash<QString, Task> tasks; // 键 → 任务（含已排队的与当前批次的）
    QStringList order;          // 入队顺序（去重由 tasks 保证）
    QSet<QString> failed;       // 本进程生命周期内失败过的键，不再重试
    QTimer batchTimer;
    QProcess *process = nullptr;
    QStringList groupKeys; // 当前批次的键（与 group 的页序一一对应）
    QString groupDir;      // 当前批次的临时目录
    QString cachedDir;     // 注入的缓存目录；空表示用运行目录旁的默认位置
    bool disabled = false;
    int batchDelayMs = 30;
    int runCount = 0;
    QString program = QStringLiteral("typst");
};

#endif // TYPSTIMAGECACHE_H
