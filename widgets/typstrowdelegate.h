#ifndef TYPSTROWDELEGATE_H
#define TYPSTROWDELEGATE_H

#include <QList>
#include <QPixmap>
#include <QStyledItemDelegate>

class QTreeView;

// 知识树行渲染：把行文本里的 typst 片段（$ … $）就地画成图片，其余文本照旧由本类绘制。
//   · 与答案区同一视觉效果：图片与选中框都不加内边距，公式在行内垂直居中（不跟文字基线对齐）；
//   · 行高只有下限（字体行高）、无上限——公式里用 typst 的 \ 换行成多行时，行随之变高；
//   · 只作用于第 0 列（到期列等仍走默认绘制）；图片未就绪或 typst 不可用时按原样显示 $ … $ 文本。
class TypstRowDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit TypstRowDelegate(QTreeView *view);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    struct Segment
    {
        QString text; // image 为空时按普通文本画
        QPixmap image;
    };

    QList<Segment> segmentsOf(const QString &text, int pointSize, int ppi) const; // 按 $ … $ 切段，公式段取图（未就绪则退回文本）

    QTreeView *treeView = nullptr; // 仅为 imageReady 后重排行高
};

#endif // TYPSTROWDELEGATE_H
