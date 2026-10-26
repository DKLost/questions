#include "typstrowdelegate.h"

#include "services/typstimagecache.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTreeView>

namespace {
const QChar kMathDelimiter = QLatin1Char('$'); // 行内 typst 片段分隔符（与题面公式录入同口径，见 htmltypstadddialog）
} // namespace

TypstRowDelegate::TypstRowDelegate(QTreeView *view)
    : QStyledItemDelegate(view)
    , treeView(view)
{
    // 图片是异步编译的：落盘后重排一次（行高可能变），让新图立刻出现在可见行上
    connect(TypstImageCache::instance(), &TypstImageCache::imageReady, this, [this]() {
        if(!treeView)
            return;
        treeView->doItemsLayout(); // 重算行高与行矩形（sizeHint 已按新图给出）
        treeView->viewport()->update();
    });
}

void TypstRowDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    opt.text.clear(); // 背景/选中/焦点仍交给样式，文字与公式由本类绘制（清空避免样式再画一遍）
    opt.icon = QIcon();
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QString text = index.data(Qt::DisplayRole).toString();
    if(text.isEmpty())
        return;

    const int ppi = TypstImageCache::ppiOf(opt.widget);
    const QList<Segment> segments = segmentsOf(text, TypstImageCache::pointSizeOf(opt.font), ppi);
    const QFontMetrics fm(opt.font);
    // 文字色统一用 Text：本平台样式（windowsvista）画选中行时是「浅蓝底 + 深色字」，
    // 并不用 palette 里那个白色的 HighlightedText，故选中行不能改用它，否则白字压浅底看不见。
    const QColor textColor = opt.palette.color(QPalette::Text);

    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true); // 超采样图缩回显示尺寸
    painter->setClipRect(opt.rect);                                // 行宽不足时截断，不越出本行
    painter->setFont(opt.font);
    painter->setPen(textColor);
    int x = opt.rect.left(); // 无内边距：内容从行矩形左缘起画
    for(const Segment &segment : segments)
    {
        if(segment.image.isNull())
        {
            const int available = qMax(0, opt.rect.right() - x);
            const int textWidth = fm.horizontalAdvance(segment.text);
            const QString shown = textWidth > available ? fm.elidedText(segment.text, Qt::ElideRight, available)
                                                        : segment.text;
            painter->drawText(QRect(x, opt.rect.top(), fm.horizontalAdvance(shown), opt.rect.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, shown);
            x += fm.horizontalAdvance(shown);
        } else
        {
            const QSize size = TypstImageCache::displaySizeOf(segment.image, ppi);
            painter->drawPixmap(QRect(QPoint(x, opt.rect.center().y() - size.height() / 2), size), segment.image);
            x += size.width();
        }
    }
    painter->restore();
}

QSize TypstRowDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const QSize base = QStyledItemDelegate::sizeHint(option, index);
    const QString text = index.data(Qt::DisplayRole).toString();
    if(text.isEmpty())
        return base;

    const int ppi = TypstImageCache::ppiOf(option.widget);
    const QFontMetrics fm(option.font);
    int width = 0;
    int height = fm.height(); // 下限＝字体行高；不设上限，多行公式把行撑高
    for(const Segment &segment : segmentsOf(text, TypstImageCache::pointSizeOf(option.font), ppi))
    {
        if(segment.image.isNull())
            width += fm.horizontalAdvance(segment.text);
        else
        {
            const QSize size = TypstImageCache::displaySizeOf(segment.image, ppi);
            width += size.width();
            height = qMax(height, size.height());
        }
    }
    return QSize(qMax(base.width(), width), qMax(base.height(), height));
}

// 按 $ … $ 切段：未闭合的 $ 视作普通文本（不吞掉后半行）
QList<TypstRowDelegate::Segment> TypstRowDelegate::segmentsOf(const QString &text, int pointSize, int ppi) const
{
    QList<Segment> segments;
    int pos = 0;
    while(pos < text.size())
    {
        const int open = text.indexOf(kMathDelimiter, pos);
        if(open < 0)
            break;
        const int close = text.indexOf(kMathDelimiter, open + 1);
        if(close < 0)
            break;
        if(open > pos)
            segments.append(Segment{text.mid(pos, open - pos), QPixmap()});

        const QString body = text.mid(open, close - open + 1); // 含两侧 $：与题面公式录入的 typst 源码一致
        const QPixmap image = TypstImageCache::instance()->pixmap(body, pointSize, ppi);
        segments.append(Segment{body, image}); // 图未就绪（首次遇到/编译失败）时按原文本显示
        pos = close + 1;
    }
    if(pos < text.size())
        segments.append(Segment{text.mid(pos), QPixmap()});
    return segments;
}
