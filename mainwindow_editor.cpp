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
#include "services/answerstore.h"

#include <QJsonDocument>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTextDocument>
#include <QUrl>

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
    const QString html = ui->questionTextEdit->toHtml();
    ToolFunctions::installScaledTypstImageResources(ui->questionTextEdit->document(), html,
                                                    ui->questionTextEdit->devicePixelRatioF());
    ui->questionTextEdit->setHtml(html);
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
    const QImage image{imageFormat.name()};
    const int scale = ToolFunctions::typstImageScale(filePath); // 2x 渲染的图按逻辑尺寸写入文档；旧图 scale=1
    const int logicalHeight = image.height() / qMax(1, scale);
    imageFormat.setWidth(qMax(1, image.width() / qMax(1, scale)));
    imageFormat.setHeight(qMax(1, logicalHeight));

    if(cursor.currentTable() != nullptr) // 表格内插入：居中并插零宽字符
    {
        ToolFunctions::centerCurrentBlockInTable(cursor);
        imageFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    } else if(logicalHeight <= kTypstSmallImageHeight)
    {
        imageFormat.setVerticalAlignment(QTextCharFormat::AlignBaseline);
    } else
    {
        imageFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    }

    // 新插入/替换的 typst 图也先按 DPR 预缩成文档资源，避免 QTextEdit 非平滑缩放源图
    if(!image.isNull())
    {
        const qreal dpr = ui->questionTextEdit->devicePixelRatioF();
        const QSize target(qMax(1, qRound(imageFormat.width() * dpr)),
                           qMax(1, qRound(imageFormat.height() * dpr)));
        ui->questionTextEdit->document()->addResource(
            QTextDocument::ImageResource, QUrl(imageFormat.name()),
            image.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
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

namespace {

QString readUtf8FileForRerender(const QString &path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

// 用 typst 源码重编译到目标路径；先写临时文件，成功后再替换，避免失败时丢掉旧图
bool compileTypstBodyTo(const QString &body, const QString &target)
{
    QString source = body.trimmed();
    if(source.isEmpty())
        return false;
    if(!(source.startsWith(QChar('$')) && source.endsWith(QChar('$'))))
        source = QStringLiteral("$ ") + source + QStringLiteral(" $");

    const QString inputPath = QStringLiteral("./data/.rerender.typ");
    const QString outputPath = QStringLiteral("./data/.rerender.png");
    const QString backupPath = target + QStringLiteral(".old");
    ToolFunctions::write_file(inputPath, ToolFunctions::typstMathPrefix + source);
    QFile::remove(outputPath);
    if(!ToolFunctions::compile_typst(inputPath, outputPath))
        return false;

    QFile::remove(backupPath);
    if(QFile::exists(target) && !QFile::rename(target, backupPath))
        return false;
    if(!QFile::rename(outputPath, target))
    {
        if(QFile::exists(backupPath))
            QFile::rename(backupPath, target);
        return false;
    }
    QFile::remove(backupPath);
    ToolFunctions::markTypstImageScale(target);
    return true;
}

QString withTypstImageSize(QString tag, const QSize &size)
{
    tag.remove(QRegularExpression(QStringLiteral(R"re(\s+width="[^"]*")re")));
    tag.remove(QRegularExpression(QStringLiteral(R"re(\s+height="[^"]*")re")));
    tag.replace(QRegularExpression(QStringLiteral(R"re(^<img\b)re")),
                QStringLiteral("<img width=\"%1\" height=\"%2\"").arg(size.width()).arg(size.height()));
    return tag;
}

} // namespace

void MainWindow::rerenderTypstImages()
{
    if(currentQuestionId > 0)
        saveCurrentQuestionContent(); // 先落盘当前编辑内容，避免重渲染时被磁盘旧 HTML 覆盖
    QApplication::setOverrideCursor(Qt::WaitCursor);
    int questionImages = 0;
    int answerImages = 0;
    int failed = 0;
    int skipped = 0;

    const QRegularExpression imgRe(QStringLiteral(R"re(<img\b[^>]*\bsrc="([^"]+)"[^>]*>)re"));
    const QRegularExpression pngSuffix(QStringLiteral(R"re(\.png$)re"));
    QDir dataDir(QStringLiteral("./data"));
    const QStringList dirs = dataDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for(const QString &dirName : dirs)
    {
        bool idOk = false;
        const int questionId = dirName.toInt(&idOk);
        if(!idOk)
            continue;

        // 题面公式：有 .typmath 源的旧图按原逻辑尺寸重渲染，并回写 width/height
        const QString htmlPath = QStringLiteral("./data/%1/question.html").arg(dirName);
        if(QFile::exists(htmlPath))
        {
            const QString html = readUtf8FileForRerender(htmlPath);
            QString rebuilt;
            int last = 0;
            bool changed = false;
            auto matches = imgRe.globalMatch(html);
            while(matches.hasNext())
            {
                const QRegularExpressionMatch match = matches.next();
                rebuilt += html.mid(last, match.capturedStart() - last);
                last = match.capturedEnd();

                QString tag = match.captured(0);
                const QString path = match.captured(1);
                if(!path.endsWith(QStringLiteral(".png")))
                {
                    rebuilt += tag;
                    continue;
                }

                QString sourcePath = path;
                sourcePath.replace(pngSuffix, QStringLiteral(".typmath"));
                if(!QFile::exists(sourcePath))
                {
                    ++skipped; // 无源码的旧图（如早期手工/生成图）无法恢复细节
                    rebuilt += tag;
                    continue;
                }

                const QImage oldImage(path);
                if(oldImage.isNull())
                {
                    ++skipped;
                    rebuilt += tag;
                    continue;
                }

                const int scale = ToolFunctions::typstImageScale(path);
                if(scale < ToolFunctions::kTypstSaveSupersample)
                {
                    const QString body = readUtf8FileForRerender(sourcePath);
                    if(compileTypstBodyTo(body, path))
                    {
                        // 题面保持旧的逻辑尺寸，只提升像素密度，避免布局跳动
                        tag = withTypstImageSize(tag, oldImage.size());
                        ++questionImages;
                        changed = true;
                    } else
                    {
                        ++failed;
                    }
                } else if(!tag.contains(QStringLiteral("width=")))
                {
                    tag = withTypstImageSize(tag, QSize(qMax(1, oldImage.width() / scale),
                                                        qMax(1, oldImage.height() / scale)));
                    changed = true;
                }
                rebuilt += tag;
            }
            rebuilt += html.mid(last);

            if(changed)
            {
                const QString backupPath = htmlPath + QStringLiteral(".bak");
                if(!QFile::exists(backupPath))
                    QFile::copy(htmlPath, backupPath);
                questionSql->write_questionHTML(questionId, rebuilt);
            }
        }

        // 答案图：auto(typst) 的 content 就是源码，直接重编译；显示端按 96 逻辑 PPI 放大到与 UI 一致
        const QString answerPath = QStringLiteral("./data/%1/answer.json").arg(dirName);
        const QString answerJson = readUtf8FileForRerender(answerPath);
        if(answerJson.isEmpty())
            continue;
        const QJsonArray answers = QJsonDocument::fromJson(answerJson.toUtf8()).array();
        for(const QJsonValue &value : answers)
        {
            const QJsonObject answer = value.toObject();
            if(answer.value(AnswerStore::FieldType).toString() != AnswerStore::TypeAutoTypst)
                continue;
            const QString content = answer.value(AnswerStore::FieldContent).toString();
            if(content.isEmpty())
                continue;
            const int answerId = answer.value(AnswerStore::FieldId).toInt();
            const QString imagePath = QStringLiteral("./data/%1/%2.png").arg(dirName).arg(answerId);
            if(ToolFunctions::typstImageScale(imagePath) >= ToolFunctions::kTypstSaveSupersample)
                continue;
            if(compileTypstBodyTo(content, imagePath))
                ++answerImages;
            else
                ++failed;
        }
    }

    QFile::remove(QStringLiteral("./data/.rerender.typ"));
    QFile::remove(QStringLiteral("./data/.rerender.png"));
    QApplication::restoreOverrideCursor();

    if(currentQuestionId > 0)
    {
        loadQuestionIntoEditor(currentQuestionId);
        load_answer(currentQuestionId);
    }

    QString message = QStringLiteral("题面公式：%1 张\n答案图：%2 张\n失败：%3 张\n无源码跳过：%4 张")
                          .arg(questionImages).arg(answerImages).arg(failed).arg(skipped);
    if(questionImages + answerImages + failed == 0)
        message += QStringLiteral("\n\n没有需要重渲染的旧图（或 typst 不可用）。");
    QMessageBox::information(this, QStringLiteral("重新渲染 typst 图片"), message);
}
