#ifndef QUESTIONPREVIEW_H
#define QUESTIONPREVIEW_H

#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QString>

class QTextCursor;
class QTextDocument;
class QWidget;
class QuestionSql;

// 题面预览：把空位替换为绿色答案（主界面=全部答案看全貌；复习窗=只填未到期，已到期留白由用户自己答）
namespace QuestionPreview {

QString answerImagePath(int questionId, int answerId); // ./data/<题>/<答案>.png

// 单处空位 → 绿色答案（图片/公式走原生图片插入）
void insertAnswerAtBlank(QTextCursor &blankCursor, const QJsonObject &answer, const QString &derivedAbs,
                         QTextDocument *document, int questionId);

void fillAllAnswers(QuestionSql *questionSql, int questionId, const QString &derivedAbs, QTextDocument *document);
void fillNotDueAnswers(QuestionSql *questionSql, int questionId, const QString &derivedAbs, QTextDocument *document);

// 弹出式题目预览窗池：只读显示题目页（未到期空位填绿），一题一窗、同题复用、新窗逐个错开。
// 主界面（ctrl+右键 / 右键菜单「弹出题目预览」）与复习窗（右键，含按住右键滑动）共用。
class WindowPool
{
public:
    explicit WindowPool(QWidget *owner); // owner 为新窗的父（随其释放），也是错开定位的基准

    QWidget *showFor(QuestionSql *questionSql, int questionId, const QString &derivedAbs); // 开/复用该题窗并显示
    QWidget *showIn(QWidget *window, QuestionSql *questionSql, int questionId, const QString &derivedAbs); // 指定窗填内容（window 为空则同 showFor）

private:
    QWidget *createWindow();

    QWidget *owner = nullptr;
    QHash<int, QPointer<QWidget>> windows; // 题 id → 窗口（QPointer 自动感知关窗）
    int cascadeSlot = 0;                   // 已开窗计数：新窗按它阶梯错开位置
};

}

#endif // QUESTIONPREVIEW_H