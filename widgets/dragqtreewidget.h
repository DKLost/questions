#ifndef DRAGQTREEWIDGET_H
#define DRAGQTREEWIDGET_H

#include <QDropEvent>
#include <QTreeWidget>

class DragQTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    DragQTreeWidget(QWidget *parent = nullptr);
signals:
    void signalRowsMoved(int start, int end);

protected:
    // 仅按根节点重排并在基类完成拖拽后通知外部；将来支持子节点拖拽时可在信号中补充父节点
    void dropEvent(QDropEvent *event) override
    {
        const QModelIndex dropIndex = indexAt(event->pos());
        if(!dropIndex.isValid()) // 落点无效：忽略本次拖拽
        {
            event->setDropAction(Qt::IgnoreAction);
            return;
        }
        if(dropIndicatorPosition() == QAbstractItemView::OnItem) // 落在节点本身不做重排
            return;

        const int startRow = selectedIndexes().first().row();
        const int endRow = dropEndRow(dropIndex);

        QTreeWidget::dropEvent(event); // 先让基类完成界面拖拽

        if(startRow != endRow)
            emit signalRowsMoved(startRow, startRow < endRow ? endRow - 1 : endRow);
    }

private:
    int dropEndRow(const QModelIndex &dropIndex) const // 落点在节点偏上/偏下对应不同插入行
    {
        if(dropIndicatorPosition() == QAbstractItemView::BelowItem)
            return dropIndex.row() + 1;
        return dropIndex.row();
    }
};

#endif // DRAGQTREEWIDGET_H