#ifndef DRAGQTREEWIDGET_H
#define DRAGQTREEWIDGET_H

#include <QTreeWidget>
#include <QDropEvent>

class DragQTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    DragQTreeWidget(QWidget *parent = nullptr);
signals:
    void signalRowsMoved(int start,int end);
protected:
    void dropEvent(QDropEvent* event)
    {
        QModelIndex index = indexAt(event->pos());//拖拽的目标位置
        if (!index.isValid()) {  // just in case
            event->setDropAction(Qt::IgnoreAction);
            return;
        }

        const QList<QModelIndex> idxs = selectedIndexes();//起始位置
        int startIndex = idxs[0].row();

        //如果鼠标放置的位置在节点上，偏上一点和偏下一点结果不同，需要判断一下
        int endIndex;
        DropIndicatorPosition dropindicationPos = dropIndicatorPosition();
        switch (dropindicationPos) {
        case QAbstractItemView::AboveItem:
            endIndex = index.row();
            break;
        case QAbstractItemView::BelowItem:
            endIndex = index.row() + 1;
            break;
        case QAbstractItemView::OnItem:
            return;
            break;
        case QAbstractItemView::OnViewport:
            break;
        }
        //执行原本的拖拽操作，界面发生变化
        QTreeWidget::dropEvent(event);

        if(startIndex != endIndex)
        {
            if (startIndex < endIndex)
                endIndex--;
            //发射移动信号，这里只设定了根节点拖拽，如果子节点也拖拽，可以信号参数中添加父节点
            emit signalRowsMoved(startIndex, endIndex);
        }
    }
};

#endif // DRAGQTREEWIDGET_H
