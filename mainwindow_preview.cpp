#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mainwindow_internal.h"

#include "services/questionpreview.h"

#include <QFile>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSqlQuery>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextImageFormat>

namespace {
// 双击命中图片时取出"选中该图片字符"的光标与图片格式（光标可能贴在图片左/右两侧，两侧都算命中）
QTextCursor imageCharAt(const QTextCursor &cursor, QTextImageFormat *imageFormat)
{
    const QTextBlock block = cursor.block();
    for(QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it)
    {
        const QTextFragment fragment = it.fragment();
        if(!fragment.charFormat().isImageFormat())
            continue;
        const int start = fragment.position();
        const int end = start + fragment.length();
        if(cursor.position() != start && cursor.position() != end)
            continue;
        *imageFormat = fragment.charFormat().toImageFormat();
        QTextCursor imageCursor(cursor.document());
        imageCursor.setPosition(start);
        imageCursor.setPosition(end, QTextCursor::KeepAnchor);
        return imageCursor;
    }
    return {};
}
} // namespace

void MainWindow::on_questionPrevieButton_toggled(bool checked)
{
    if(checked)
        enterQuestionPreview();
    else
        leaveQuestionPreview();
}

// 进入预览：克隆真实文档 → 在克隆上填满全部答案 → 切到克隆只读显示（真实文档原封不动，故退出时零格式漂移）
void MainWindow::enterQuestionPreview()
{
    if(questionPreviewActive)
        return;
    if(editorQuestionId <= 0) // 当前没有题目：按钮弹回，不进入预览
    {
        ui->questionPrevieButton->setChecked(false);
        return;
    }
    realQuestionDocument = ui->questionTextEdit->document();
    realQuestionDocument->setParent(this); // 接过所有权：编辑器控件换文档时会销毁"它自己那份"文档
    previewQuestionDocument = realQuestionDocument->clone(this); // 存于本窗口名下，退出预览时销毁
    previewQuestionDocument->setIndentWidth(kEditorIndentWidth); // clone 后与真实文档同排版
    const QString derivedAbs = questionSql->get_value(kQuestionsTable, editorQuestionId, kAbstractNameField).toString();
    QuestionPreview::fillAllAnswers(questionSql, editorQuestionId, derivedAbs, previewQuestionDocument);
    questionPreviewActive = true;
    const QSignalBlocker blocker(ui->questionTextEdit);
    ui->questionTextEdit->setReadOnly(true);
    ui->questionTextEdit->setDocument(previewQuestionDocument); // 换文档显示：题面不经 HTML 往返，格式与撤销历史都不受影响
}

// 退出预览：切回真实文档并恢复编辑，销毁预览文档（保存/换题/用户取消勾选都从这里收口）
void MainWindow::leaveQuestionPreview()
{
    if(!questionPreviewActive)
        return;
    questionPreviewActive = false; // 先落态：按钮弹起触发的重入在此挡住
    ui->questionPrevieButton->setChecked(false);
    {
        const QSignalBlocker blocker(ui->questionTextEdit);
        ui->questionTextEdit->setDocument(realQuestionDocument);
        ui->questionTextEdit->setReadOnly(false);
    }
    delete previewQuestionDocument;
    previewQuestionDocument = nullptr;
    realQuestionDocument = nullptr;
}

// 双击 typst 公式图片改公式：源码由同目录 .typmath 读回 → 复用 HtmlTypstAddDialog 编辑 → 按新内容重新出图并原位替换
bool MainWindow::editTypstImageAt(const QPoint &viewportPos)
{
    if(questionPreviewActive || editorQuestionId <= 0)
        return false; // 预览中（只读）或没有题目：不进入公式编辑
    QTextImageFormat imageFormat;
    QTextCursor imageCursor = imageCharAt(ui->questionTextEdit->cursorForPosition(viewportPos), &imageFormat);
    const QString sourcePath = typstSourcePathOf(imageFormat);
    if(imageCursor.isNull() || sourcePath.isEmpty())
        return false; // 不是 typst 公式图片：交给默认双击处理

    htmlTypstAddDialog->reset();
    htmlTypstAddDialog->setQId(editorQuestionId);
    htmlTypstAddDialog->setText(readTextFile(sourcePath)); // 预置原公式源码
    const bool accepted = htmlTypstAddDialog->exec() == QDialog::Accepted;
    const QString newFilePath = htmlTypstAddDialog->getRetPath();
    htmlTypstAddDialog->reset();
    if(!accepted || newFilePath.isEmpty())
        return true; // 取消：双击已被消费，不再落到默认的"双击选词"
    imageCursor.removeSelectedText(); // 原位替换：删掉旧图字符，按既有插入规则放入新编译的图
    ui->questionTextEdit->setTextCursor(imageCursor);
    insertTypstImage(newFilePath);
    return true;
}