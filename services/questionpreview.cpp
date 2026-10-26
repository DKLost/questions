#include "questionpreview.h"
#include "core/questionsql.h"
#include "core/knowledgedb.h"
#include "answerstore.h"
#include "utils/toolfunctions.h"

#include <QDate>
#include <QImage>
#include <QJsonArray>
#include <QScreen>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextImageFormat>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {
const QString kClozesTable = QStringLiteral("clozes");
const QString kQuestionsTable = QStringLiteral("questions");
const QString kNameField = QStringLiteral("name");
const QString kNextDateField = QStringLiteral("nextDate");
const QString kDateStorageFormat = QStringLiteral("yyyy/MM/dd"); // nextDate 在库中的存储格式
const QString kGreenAnswerColor = QStringLiteral("#4CAF50");
const QString kImagePlaceholderText = QStringLiteral("[图片]");
constexpr int kMaxInlineImageWidth = 400;
constexpr int kInlineImageBorderWidth = 2;
const QString kChainTypstInputPath = QStringLiteral("chaintypst_preview.typ");  // 临时编译输入
const QString kChainTypstOutputPath = QStringLiteral("chaintypst_preview.png"); // 临时编译输出

// 弹出式预览窗：尺寸与"新窗逐个错开"的步长（以 owner 左上角为基准）
constexpr int kPreviewWindowWidth = 640;
constexpr int kPreviewWindowHeight = 560;
constexpr int kPreviewCascadeStartX = 60;
constexpr int kPreviewCascadeStartY = 50;
constexpr int kPreviewCascadeStepX = 34;
constexpr int kPreviewCascadeStepY = 26;
constexpr int kPreviewCascadeSlots = 6;

QTextCharFormat greenAnswerFormat() // 预览答案的绿色强调文本格式
{
    QTextCharFormat format;
    format.setForeground(QColor(kGreenAnswerColor));
    format.setFontUnderline(false);
    return format;
}

// 链卡(typst)回填：图谱末段宾词是 `$ … $` 公式源、没有落盘图，就地按题面公式同口径编译出临时 PNG 再内联插入
QString compileChainTypstPreview(const QString &body)
{
    ToolFunctions::write_file(kChainTypstInputPath, ToolFunctions::typstMathPrefix + body);
    ToolFunctions::compile_typst(kChainTypstInputPath, kChainTypstOutputPath);
    return kChainTypstOutputPath;
}

// 空位是否已到期：与复习集筛选同一口径（nextDate 同格式存储，文本序即日期序）
bool isClozeDue(QuestionSql *questionSql, int answerId)
{
    const QString nextDate = questionSql->get_value(kClozesTable, answerId, kNextDateField).toString();
    return nextDate <= QDate::currentDate().toString(kDateStorageFormat);
}

// 逐空回填：notDueOnly = 只填未到期的空（已到期的空留白，由用户自己答）
void fillAnswers(QuestionSql *questionSql, int questionId, const QString &derivedAbs, QTextDocument *document, bool notDueOnly)
{
    QTextCursor cursor(document);
    const QJsonArray answers = questionSql->read_answerJSON(questionId);
    for(int row = 0; row < answers.count(); ++row)
    {
        const QJsonObject answer = answers[row].toObject();
        if(notDueOnly && isClozeDue(questionSql, answer.value(AnswerStore::FieldId).toInt()))
            continue;
        for(QTextCursor &blankCursor : ToolFunctions::find_blanks_by_number(row + 1, cursor)) // 填空编号从 1 开始
            QuestionPreview::insertAnswerAtBlank(blankCursor, answer, derivedAbs, document, questionId);
    }
}
}

QString QuestionPreview::answerImagePath(int questionId, int answerId)
{
    return QStringLiteral("./data/%1/%2.png").arg(questionId).arg(answerId);
}

// 把一处空位替换为绿色答案：图片/公式走原生图片插入（带绿色边框），其余为绿色文本
void QuestionPreview::insertAnswerAtBlank(QTextCursor &blankCursor, const QJsonObject &answer, const QString &derivedAbs,
                                         QTextDocument *document, int questionId)
{
    ToolFunctions::select_current_underline_text(&blankCursor);
    const QString type = answer.value(AnswerStore::FieldType).toString();
    const int answerId = answer.value(AnswerStore::FieldId).toInt();
    QString content = answer.value(AnswerStore::FieldContent).toString();
    if(AnswerStore::isChainCardType(type))
        content = KnowledgeDB::lastObjectOfDerivedAbs(derivedAbs); // 链卡：以挂载的 DerivedAbs 末段名显示（链卡(typst) 即 `$ … $` 公式源）

    QString imagePath; // 需要内联成图的三种类型；其余走绿色文本
    if(type == AnswerStore::TypeManualImage)
        imagePath = content;
    else if(type == AnswerStore::TypeAutoTypst)
        imagePath = answerImagePath(questionId, answerId);
    else if(type == AnswerStore::TypeAutoChainTypst && KnowledgeDB::isChainTypstName(content))
        imagePath = compileChainTypstPreview(content); // 宾词名是 `$ … $`：就地编译成图（`$ … $ 混排`暂不支持，退回文本）
    else
    {
        blankCursor.insertText(content, greenAnswerFormat());
        return;
    }

    const QImage image = ToolFunctions::scaledWithBorder(QImage(imagePath), kMaxInlineImageWidth,
                                                         kInlineImageBorderWidth, QColor(kGreenAnswerColor));
    if(image.isNull()) // 图片加载失败：文本占位
    {
        blankCursor.insertText(type == AnswerStore::TypeManualImage ? kImagePlaceholderText : content, greenAnswerFormat());
        return;
    }
    const QString resourceName = QStringLiteral("previewanswer_%1_%2").arg(questionId).arg(answerId);
    document->addResource(QTextDocument::ImageResource, QUrl(resourceName), image);
    QTextImageFormat imageFormat;
    imageFormat.setName(resourceName);
    imageFormat.setWidth(image.width());
    imageFormat.setHeight(image.height());
    imageFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    blankCursor.insertImage(imageFormat);
}

void QuestionPreview::fillAllAnswers(QuestionSql *questionSql, int questionId, const QString &derivedAbs, QTextDocument *document)
{
    fillAnswers(questionSql, questionId, derivedAbs, document, false);
}

void QuestionPreview::fillNotDueAnswers(QuestionSql *questionSql, int questionId, const QString &derivedAbs, QTextDocument *document)
{
    fillAnswers(questionSql, questionId, derivedAbs, document, true);
}

// ===== 弹出式题目预览窗（主界面 ctrl+右键/菜单、复习窗右键共用）=====

QuestionPreview::WindowPool::WindowPool(QWidget *owner) : owner(owner) {}

QWidget *QuestionPreview::WindowPool::showFor(QuestionSql *questionSql, int questionId, const QString &derivedAbs)
{
    return showIn(windows.value(questionId).data(), questionSql, questionId, derivedAbs);
}

// 填内容并显示；window 为空则新建。登记随内容走：一个窗口只挂在一个题号下
QWidget *QuestionPreview::WindowPool::showIn(QWidget *window, QuestionSql *questionSql, int questionId, const QString &derivedAbs)
{
    if(!questionSql || questionId <= 0)
        return nullptr;
    if(!window)
        window = createWindow();
    if(!window)
        return nullptr;

    for(auto it = windows.begin(); it != windows.end();) // 滑动会把窗口换成别的题：登记前摘掉旧键
        it = it.value().data() == window ? windows.erase(it) : ++it;
    windows.insert(questionId, window);

    auto *browser = window->findChild<QTextBrowser *>();
    browser->setHtml(questionSql->read_questionHTML(questionId));
    QuestionPreview::fillNotDueAnswers(questionSql, questionId, derivedAbs, browser->document());
    browser->moveCursor(QTextCursor::Start); // 每次预览都从题首看起
    window->setWindowTitle(QStringLiteral("%1 · id %2")
                               .arg(questionSql->get_value(kQuestionsTable, questionId, kNameField).toString())
                               .arg(questionId));
    window->show();
    window->raise(); // 置顶但不抢焦点，免得打断按住右键的拖动浏览
    return window;
}

// 只含一个 QTextBrowser 的独立窗口：父为 owner（随其释放），关闭即销毁；新窗逐个错开不堆在正中间
QWidget *QuestionPreview::WindowPool::createWindow()
{
    if(!owner)
        return nullptr;
    auto *window = new QWidget(owner, Qt::Window);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->resize(kPreviewWindowWidth, kPreviewWindowHeight);

    // 以 owner 左上角为基准，按已开窗数阶梯右下偏移，再夹进本屏可用区
    const QRect area = window->screen() ? window->screen()->availableGeometry() : QRect();
    const int slot = cascadeSlot++ % kPreviewCascadeSlots;
    QPoint pos = owner->frameGeometry().topLeft()
                 + QPoint(kPreviewCascadeStartX + kPreviewCascadeStepX * slot,
                          kPreviewCascadeStartY + kPreviewCascadeStepY * slot);
    if(area.isValid())
    {
        pos.setX(qBound(area.left(), pos.x(), area.right() - window->width()));
        pos.setY(qBound(area.top(), pos.y(), area.bottom() - window->height()));
    }
    window->move(pos);

    auto *layout = new QVBoxLayout(window);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *browser = new QTextBrowser(window); // QTextBrowser 本身只读
    browser->setOpenExternalLinks(false);
    layout->addWidget(browser);
    return window;
}