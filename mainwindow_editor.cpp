#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mainwindow_internal.h"

#include "parser/noteparser2.h"
#include "core/knowledgedb.h"
#include "widgets/knowledgetreehelper.h"
#include "widgets/questiontableviewhelper.h"
#include "dialogs/phenadddialog.h"
#include "utils/toolfunctions.h"
#include "services/questionpreview.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QSettings>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QSize>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardItem>
#include <QStringList>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextFrameFormat>
#include <QTextImageFormat>
#include <QTextList>
#include <QTextListFormat>
#include <QTextTableFormat>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const QString kQuestionImageDirTemplate = QStringLiteral("%1/data/%2/");
const QString kDescriptionThemeSeparator = QStringLiteral("⇒");
const QString kUnorderedListType = QStringLiteral("无序列表");
const QString kOrderedListType = QStringLiteral("有序列表");
const QString kHeadingType = QStringLiteral("标题");
constexpr int kListBlockMargin = 3;
constexpr int kTypstSmallImageHeight = 16;
constexpr int kTableBorderWidth = 1;
constexpr int kTableCellPadding = 5;
const QString kAnswerGenFontFamily = QStringLiteral("Tahoma");
const QString kAnswerGenSpacer = QStringLiteral("      ");
constexpr int kAnswerGenBackSteps = 4;
const QString kAutoNumberPadding = QStringLiteral("   ");
// 在 block 末尾插入带固定上下边距的空块，返回指向新块的游标
QTextCursor insertDescriptionBlock(QTextBlock block)
{
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::EndOfBlock);
    QTextBlockFormat blockFormat;
    blockFormat.setTopMargin(kListBlockMargin);
    blockFormat.setBottomMargin(kListBlockMargin);
    cursor.insertBlock(blockFormat, QTextCharFormat());
    return cursor;
}

// 无序列表：跳过同层或更深的列表项，返回可插入的块
QTextBlock findUnorderedListInsertBlock(QTextBlock block, const QTextListFormat &format)
{
    QTextList *nextList = block.next().textList();
    while(nextList != nullptr && nextList->format().indent() >= format.indent())
    {
        block = block.next();
        nextList = block.next().textList();
    }
    return block;
}

// 有序列表：跳到同层编号列表末尾，lastDecimalBlock 出参为最后一个同层编号块
QTextBlock findOrderedListInsertBlock(QTextBlock block, const QTextListFormat &format, QTextBlock &lastDecimalBlock)
{
    QTextList *nextList = block.next().textList();
    while(nextList != nullptr && nextList->format().style() == QTextListFormat::ListDecimal &&
          nextList->format().indent() == format.indent())
    {
        block = block.next();
        lastDecimalBlock = block;
        nextList = block.next().textList();
        while(nextList != nullptr && nextList->format().indent() > format.indent())
        {
            block = block.next();
            nextList = block.next().textList();
        }
    }
    return block;
}

// 右移两格后扫描到下一个下划线起点；无法继续前进时返回 false
bool moveCursorToNextUnderline(QTextCursor &cursor)
{
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);
    bool reached = cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);
    while(!cursor.charFormat().fontUnderline())
    {
        if(!cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1))
        {
            reached = false;
            break;
        }
    }
    return reached;
}
} // namespace

void MainWindow::on_htmlImgAddButton_clicked()
{
    insertImageToQuestion(QTextCharFormat::AlignMiddle);
}

void MainWindow::on_htmlImgAddButton_2_clicked()
{
    insertImageToQuestion(QTextCharFormat::AlignBottom);
}

void MainWindow::insertImageToQuestion(QTextCharFormat::VerticalAlignment align)
{
    if(!ui->questionTableView->currentIndex().isValid())
        return;

    const int questionId = currentQuestionId;
    const QString dirPath = kQuestionImageDirTemplate.arg(QDir::currentPath()).arg(questionId);
    const QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("打开图片"), dirPath,
                                                          tr("Image Files (*.jpg *.png *.gif)"));
    if(filePath.isEmpty())
        return;

    QDir dir("./");
    QTextCursor cursor = ui->questionTextEdit->textCursor();
    QTextImageFormat imageFormat;
    imageFormat.setVerticalAlignment(align);
    imageFormat.setName(dir.relativeFilePath(filePath));
    ToolFunctions::centerCurrentBlockInTable(cursor); // 表格内插入：居中并插零宽字符，避免行塌陷
    cursor.insertImage(imageFormat);
}

void MainWindow::on_htmlTableAddButton_clicked()
{
    htmlTableAddDialog->setRetRow(0);
    htmlTableAddDialog->setRetColumn(0);
    htmlTableAddDialog->exec();
    QTextTableFormat tableFormat;
    tableFormat.setBorder(kTableBorderWidth);
    tableFormat.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
    tableFormat.setCellPadding(kTableCellPadding);
    tableFormat.setBorderCollapse(false);
    const int leftMargin = ui->questionTextEdit->cursorRect(ui->questionTextEdit->textCursor()).x();
    tableFormat.setLeftMargin(leftMargin);
    const int row = htmlTableAddDialog->getRetRow();
    const int column = htmlTableAddDialog->getRetColumn();
    ui->questionTextEdit->textCursor().insertTable(row, column, tableFormat);
}

// 自动编号下一填空9/29：返回下一个编号，找不到返回 -1
int MainWindow::autoNumberNext(QTextCursor &cursor)
{
    const int number = ToolFunctions::get_cursor_number(&cursor);
    if(number == -1)
        return -1;
    if(!moveCursorToNextUnderline(cursor))
        return -1;
    if(ToolFunctions::get_cursor_number(&cursor) != -1)
        rewriteUnderlinedNumber(cursor, number + 1);
    return number + 1;
}

// 用下划线格式重写当前位置的空（原编号已被选中）
void MainWindow::rewriteUnderlinedNumber(QTextCursor &cursor, int value)
{
    QTextCharFormat format = cursor.charFormat();
    format.setFontUnderline(true);
    cursor.removeSelectedText();
    cursor.setCharFormat(format);
    cursor.insertText(kAutoNumberPadding + QString::number(value) + kAutoNumberPadding);
    ui->questionTextEdit->setTextCursor(cursor);
}

void MainWindow::on_autoNumberButton_clicked()
{
    QTextCursor cursor = ui->questionTextEdit->textCursor();
    autoNumberNext(cursor);
}

void MainWindow::on_autoNumberAllButton_clicked()
{
    QTextCursor cursor = ui->questionTextEdit->textCursor();
    while(autoNumberNext(cursor) != -1)
    {
        cursor = ui->questionTextEdit->textCursor();
        autoNumberNext(cursor);
    }
}

// 切换选定部分下划线(快捷键F6) 9/28
void MainWindow::on_underlineToggleButton_clicked()
{
    QTextCursor cursor = ui->questionTextEdit->textCursor();

    if(!cursor.hasSelection())
        ToolFunctions::select_current_underline_text(&cursor);

    if(cursor.hasSelection())
    {
        QTextCharFormat format;
        if(cursor.position() < cursor.selectionEnd())
        {
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            format = cursor.charFormat();
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
        } else
        {
            format = cursor.charFormat();
        }
        format.setFontUnderline(!format.underlineStyle());
        cursor.mergeCharFormat(format);
    }
}

void MainWindow::on_descAddButton_clicked()
{
    descAddDialog->reset();
    QTextCursor cursor = ui->questionTextEdit->textCursor();
    descAddDialog->setThemeBeing(cursor.block().text().split(kDescriptionThemeSeparator)[0]);
    if(!descAddDialog->exec())
        return;

    const int originalPosition = ui->questionTextEdit->textCursor().position();
    const QString type = descAddDialog->getRetType();
    const QString content = descAddDialog->getRetContent();
    if(type == kUnorderedListType)
        insertUnorderedListDescription(cursor, content);
    else if(type == kOrderedListType)
        insertOrderedListDescription(cursor, content);
    else if(type == kHeadingType)
        insertHeadingDescription(cursor, content);

    cursor.setPosition(originalPosition);
    ui->questionTextEdit->setTextCursor(cursor);
}

// 无序列表：不是列表则新建，是列表则缩进一级并沿用项目符号
void MainWindow::insertUnorderedListDescription(QTextCursor cursor, const QString &content)
{
    QTextBlock block = cursor.block();
    QTextList *list = block.textList();
    QTextListFormat format;
    if(list == nullptr)
    {
        format.setStyle(QTextListFormat::ListDisc);
    } else
    {
        format = list->format();
        format.setIndent(format.indent() + 1);
        format.setStyle(QTextListFormat::ListDisc);
    }

    block = findUnorderedListInsertBlock(block, format);
    cursor = insertDescriptionBlock(block);
    cursor.createList(format);
    QTextList *blockList = block.textList();
    if(blockList != nullptr && blockList->format().style() != QTextListFormat::ListDecimal &&
       blockList->format().indent() == cursor.currentList()->format().indent())
    {
        blockList->add(cursor.block());
    }
    cursor.insertText(content);
}

// 有序列表：跳到同层编号列表末尾追加，必要时并入前一个编号列表
void MainWindow::insertOrderedListDescription(QTextCursor cursor, const QString &content)
{
    QTextBlock block = cursor.block();
    QTextList *list = block.textList();
    QTextListFormat format;
    format.setStyle(QTextListFormat::ListDecimal);
    if(list == nullptr)
    {
        format.setIndent(1);
    } else
    {
        format.setIndent(list->format().indent() + 1);
    }

    QTextBlock lastDecimalBlock;
    block = findOrderedListInsertBlock(block, format, lastDecimalBlock);
    cursor = insertDescriptionBlock(block);
    cursor.createList(format);
    QTextList *lastList = lastDecimalBlock.textList();
    if(lastList != nullptr && lastList->format().style() == QTextListFormat::ListDecimal &&
       lastList->format().indent() == cursor.currentList()->format().indent())
    {
        lastList->add(cursor.block());
    }
    cursor.insertText(content);
}

// 标题：按当前块标题层级 +1 插入，跳过层级不低于新标题的后续块
void MainWindow::insertHeadingDescription(QTextCursor cursor, const QString &content)
{
    QTextBlock block = cursor.block();
    QTextBlockFormat blockFormat;
    blockFormat.setHeadingLevel(block.blockFormat().headingLevel() + 1);

    while(block.next().isValid() &&
          (block.next().blockFormat().headingLevel() == 0 ||
           block.next().blockFormat().headingLevel() >= blockFormat.headingLevel()))
    {
        block = block.next();
    }

    cursor = QTextCursor(block);
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertBlock(blockFormat, QTextCharFormat());
    cursor.insertText(content);
    // 自我往返仅用于规整文档结构，内容未变：屏蔽信号避免触发一次多余落盘
    const QSignalBlocker blocker(ui->questionTextEdit);
    ui->questionTextEdit->setHtml(ui->questionTextEdit->toHtml());
}

void MainWindow::on_htmlTypstAddButton_clicked()
{
    htmlTypstAddDialog->reset();
    htmlTypstAddDialog->setQId(currentQuestionId);
    htmlTypstAddDialog->setText(ui->questionTextEdit->textCursor().selectedText());
    htmlTypstAddDialog->exec();
    const QString filePath = htmlTypstAddDialog->getRetPath();
    if(ui->questionTableView->currentIndex().isValid() && !filePath.isEmpty())
        insertTypstImage(filePath);
    htmlTypstAddDialog->reset();
}

// 插入 typst 预览图：table 内居中，小图基线对齐、大图居中
void MainWindow::insertTypstImage(const QString &filePath)
{
    QDir dir("./");
    QTextCursor cursor = ui->questionTextEdit->textCursor();
    QTextImageFormat imageFormat;
    imageFormat.setName(dir.relativeFilePath(filePath));
    imageFormat.setToolTip(readTextFile(typstSourcePathOf(imageFormat))); // 公式源码（同目录 .typmath）作 tooltip，便于双击回改
    const int height = QImage{imageFormat.name()}.height();

    if(cursor.currentTable() != nullptr) // 表格内插入：居中并插零宽字符
    {
        ToolFunctions::centerCurrentBlockInTable(cursor);
        imageFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    } else if(height <= kTypstSmallImageHeight)
    {
        imageFormat.setVerticalAlignment(QTextCharFormat::AlignBaseline);
    } else
    {
        imageFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    }

    cursor.insertImage(imageFormat);
}

void MainWindow::on_answerGenAddButton_clicked()
{
    QTextCursor cursor = ui->questionTextEdit->textCursor();
    QTextCharFormat format = cursor.charFormat();
    format.setFontFamilies({kAnswerGenFontFamily});
    format.setFontUnderline(true);
    format.setFontItalic(false);
    if(!cursor.selectedText().isEmpty())
    {
        on_answerAddButton_clicked();
        cursor = ui->questionTextEdit->textCursor();
        cursor.removeSelectedText();
    }
    cursor.insertText(kAnswerGenSpacer, format);
    cursor.insertText("\u200B", {});
    cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, kAnswerGenBackSteps);
    ui->questionTextEdit->setTextCursor(cursor);
}
