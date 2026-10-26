#include "learningdialog.h"
#include "ui_learningdialog.h"
#include "learningdialog_internal.h"
#include "utils/toolfunctions.h"
#include "core/fsrs.h"
#include "core/knowledgedb.h"
#include "widgets/knowledgetreehelper.h"
#include "widgets/questiontableviewhelper.h"
#include "services/questionpreview.h"
#include <QSet>
#include <QUrl>
#include <QPainter>
#include <QTextImageFormat>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QToolButton>
#include <QPushButton>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTextDocument>
#include <QTextBlock>
#include <algorithm>
#include <functional>
#include <QTextFragment>
#include <QTextDocumentFragment>

using namespace learningdialog_internal;

namespace {
constexpr int kBlankClickTolerance = 3; // 题面按下→释放的位移阈值（超过即视为拖动选择文本，不做空位定位）

// 知识树右键预览：按 DerivedAbs 查全库题目（预览不限当前复习集）
const QString kQuestionDbConnection = QStringLiteral("connection1"); // core 里 QuestionSql 的连接名
const QString kQuestionIdByDerivedAbsSql = QStringLiteral("SELECT id FROM questions WHERE abstractName = ?");

// 该现象派生的 DerivedAbs 在全库对应的题目 id
int questionIdOfDerivedAbs(const QString &derivedAbs)
{
    QSqlQuery query(QSqlDatabase::database(kQuestionDbConnection));
    query.prepare(kQuestionIdByDerivedAbsSql);
    query.addBindValue(derivedAbs);
    return query.exec() && query.next() ? query.value(0).toInt() : kNoQuestion;
}
}

// 目标空两侧邻居依次滚入视野：远端邻居优先，让目标最终落在视口内而非边缘
void LearningDialog::revealBlankNeighbors(const QTextCursor &blankCursor, int blankNumber, bool blankAfterCursor)
{
    const int farStep = blankAfterCursor ? 1 : -1;
    for(int step : {farStep, -farStep})
    {
        QTextCursor searchBase = blankCursor; // 查找基准用副本：目标光标必须保持"无选区"，否则最后定位会变成选中整段空
        QTextCursor neighbor = ToolFunctions::find_blank_by_number(blankNumber + step, searchBase);
        if(neighbor.position() == blankCursor.position()) // 该侧已无空：退到文档首尾
            neighbor.movePosition(step > 0 ? QTextCursor::End : QTextCursor::Start);
        ui->textBrowser->setTextCursor(neighbor);
        ui->textBrowser->ensureCursorVisible();
    }
}

// 先滚两侧邻居再定位目标：避免目标被留在视口边缘
void LearningDialog::highlight_blank_by_number(int blankNumber)
{
    QTextCursor cursor = ui->textBrowser->textCursor();
    if(ToolFunctions::get_cursor_number(&cursor) == blankNumber)
        return;
    QTextCursor highlightCursor = ToolFunctions::find_blank_by_number(blankNumber, cursor);
    revealBlankNeighbors(highlightCursor, blankNumber, highlightCursor.position() > cursor.position());
    if(cursor.position() != highlightCursor.position())
        ui->textBrowser->setTextCursor(highlightCursor);
}

bool LearningDialog::eventFilter(QObject *watched, QEvent *event)
{
    if(interceptLockedRowExpand(watched, event))
        return true;
    if(previewPhenomenonOnRightClick(watched, event))
        return true;
    if(focusFirstAnswerInputOnTab(watched, event))
        return true;
    if(focusAnswerRowFromBlankClick(watched, event))
        return true;
    handleBlankHighlightEvent(watched, event);
    return QDialog::eventFilter(watched, event);
}

// Tab（焦点在题目表/知识树上时）：交给首个可作答输入框，省去"切题后先点一下第一个空"
bool LearningDialog::focusFirstAnswerInputOnTab(QObject *watched, QEvent *event)
{
    if(event->type() != QEvent::KeyPress || (watched != ui->tableView && watched != ui->phenTreeView))
        return false;
    const QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if(keyEvent->key() != Qt::Key_Tab || keyEvent->modifiers() != Qt::NoModifier)
        return false;
    QLineEdit *input = firstAnswerInput();
    if(!input)
        return false;
    input->setFocus();
    return true;
}

QLineEdit *LearningDialog::firstAnswerInput() const
{
    for(int row = 0; row < answerRowCount; ++row)
    {
        AnswerRowControls controls = answerRowAt(row);
        if(controls.input && controls.input->isVisible() && controls.input->isEnabled()) // 未到期/已锁定/被门控隐藏的行都不算
            return controls.input;
    }
    return nullptr;
}

// 题面点空位 → 定位对应答案行（反向联动；正向是输入框聚焦时高亮题面空位）
bool LearningDialog::focusAnswerRowFromBlankClick(QObject *watched, QEvent *event)
{
    if(watched != ui->textBrowser && watched != ui->textBrowser->viewport())
        return false;
    if(event->type() == QEvent::MouseButtonPress)
    {
        blankPressPos = static_cast<QMouseEvent *>(event)->position().toPoint();
        return false; // 只记录起点：后续位移超过阈值即按"拖动选择文本"处理
    }
    if(event->type() != QEvent::MouseButtonRelease)
        return false;
    const QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    const QPoint releasePos = mouseEvent->position().toPoint();
    if(mouseEvent->button() != Qt::LeftButton || (releasePos - blankPressPos).manhattanLength() > kBlankClickTolerance)
        return false;
    QTextCursor cursor = ui->textBrowser->cursorForPosition(releasePos);
    const int blankNumber = ToolFunctions::get_cursor_number(&cursor); // 该调用同时把 cursor 扩展为"选中整个空"
    if(blankNumber <= 0)
        return false; // 无编号空 / 不在空上
    AnswerRowControls controls = answerRowAt(blankNumber - 1); // 空位编号从 1 开始
    if(!controls.input || !controls.input->isVisible() || !controls.input->isEnabled())
        return false;
    ui->textBrowser->setTextCursor(cursor); // 题面把该空显示为选中（即界面上的高亮）
    ui->scrollArea->ensureWidgetVisible(controls.input);
    controls.input->setFocus();
    return true; // 吞掉释放事件，避免默认处理清掉刚建立的空位选区
}

// 知识树右键：按下该行即预览，按住右键继续滑过其他行也逐行预览（同一行不重复回填）；吞掉事件，右键不改选中行、不弹菜单
bool LearningDialog::previewPhenomenonOnRightClick(QObject *watched, QEvent *event)
{
    if(watched != ui->phenTreeView->viewport())
        return false;
    if(event->type() == QEvent::MouseButtonRelease)
    {
        rightDragPreviewDerivedAbs.clear(); // 一次右键手势结束
        return false;
    }
    const QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if(event->type() == QEvent::MouseButtonPress)
    {
        if(mouseEvent->button() != Qt::RightButton)
            return false;
    } else if(event->type() == QEvent::MouseMove)
    {
        if(!mouseEvent->buttons().testFlag(Qt::RightButton))
            return false; // 未按住右键的移动（普通悬停）放行
    } else
        return false;

    const QString derivedAbs = questionDerivedAbsForIndex(ui->phenTreeView, ui->phenTreeView->indexAt(mouseEvent->position().toPoint()));
    const bool freshPress = event->type() == QEvent::MouseButtonPress;
    if(!derivedAbs.isEmpty() && (freshPress || derivedAbs != rightDragPreviewDerivedAbs)) // 按下必刷新；滑动只换行才刷新
    {
        rightDragPreviewDerivedAbs = derivedAbs;
        showQuestionPreviewForDerivedAbs(derivedAbs, freshPress); // 无对应题目时保持原样
    }
    return true; // 吞掉按下/拖动：既不改变选中行也不弹右键菜单
}

// 右键预览：为该题开/复用一个独立预览窗（窗池：同题复用，可同时开多个），只读显示该现象的题目页，
// 并把未到期的空位替换为绿色答案（已到期的空保持空白，避免看到当前要答的答案）。
// freshPress＝右键刚按下：用/开该题自己的窗口，并记为本次手势的窗口；否则（按住右键滑动）
// 只把本手势那个窗口换成当前行，避免滑过一串行弹出一堆窗。
void LearningDialog::showQuestionPreviewForDerivedAbs(const QString &derivedAbs, bool freshPress)
{
    if(freshPress)
        dragPreviewWindow = nullptr; // 新手势先解绑上一个
    const int questionId = questionIdOfDerivedAbs(derivedAbs);
    if(questionId == kNoQuestion)
        return; // 该现象没有对应题目：保持原样（本次手势的窗口也不动）

    QWidget *window = freshPress ? previewWindows.showFor(questionSql, questionId, derivedAbs)
                                 : previewWindows.showIn(dragPreviewWindow.data(), questionSql, questionId, derivedAbs);
    if(window && freshPress)
        dragPreviewWindow = window;
}

// 门控行展开锁定守卫：拦下全部展开入口（展开指示器按下、行双击、中键、键盘 → / * / +）
bool LearningDialog::interceptLockedRowExpand(QObject *watched, QEvent *event)
{
    const bool mouseOnViewport = watched == ui->phenTreeView->viewport()
                                 && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick);
    if(mouseOnViewport)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        const QModelIndex index = ui->phenTreeView->indexAt(mouseEvent->position().toPoint());
        if(!isDerivedAbsLocked(index))
            return false;
        if(event->type() == QEvent::MouseButtonDblClick || mouseEvent->button() == Qt::MiddleButton)
            return true; // 双击 = 切换展开；中键 = 展开整棵子树
        return mouseEvent->button() == Qt::LeftButton && mouseOnExpandIndicator(index, mouseEvent->position().x());
    }
    if(watched == ui->phenTreeView && event->type() == QEvent::KeyPress)
    {
        const int key = static_cast<QKeyEvent *>(event)->key();
        return (key == Qt::Key_Right || key == Qt::Key_Asterisk || key == Qt::Key_Plus)
               && isDerivedAbsLocked(ui->phenTreeView->currentIndex());
    }
    return false;
}

bool LearningDialog::mouseOnExpandIndicator(const QModelIndex &index, int x) const
{
    int depth = 0;
    for(QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent())
        ++depth;
    const int indent = ui->phenTreeView->indentation();
    return x >= depth * indent && x < (depth + 1) * indent; // 展开指示器即行首第 depth 层缩进的方块
}

// 题面空位高亮联动：输入框获得焦点/划过时把它对应的空滚到视野中
void LearningDialog::handleBlankHighlightEvent(QObject *watched, QEvent *event)
{
    static bool blankHighlighted = false; // 同一输入框聚焦后悬停不再重复滚动
    if(answerStage != AnswerStage::Waiting || gateActive) // 已提交或门控演练时不做联动
    {
        blankHighlighted = false;
        return;
    }
    QLineEdit *input = qobject_cast<QLineEdit *>(watched);
    if(!input)
        return;
    if(event->type() == QEvent::FocusIn)
    {
        if(!input->toolTip().isEmpty())
        {
            highlight_blank_by_number(input->toolTip().toInt() + 1); // 空位编号从 1 开始
            blankHighlighted = true;
        }
    } else if(event->type() == QEvent::FocusOut)
    {
        blankHighlighted = false;
    } else if(event->type() == QEvent::MouseMove && !blankHighlighted)
    {
        highlight_blank_by_number(input->toolTip().toInt() + 1);
    }
}