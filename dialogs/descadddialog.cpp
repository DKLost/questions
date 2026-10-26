#include "descadddialog.h"
#include "ui_descadddialog.h"

#include <QLineEdit>
#include <QFontMetrics>

namespace
{
constexpr int kTextSidePadding = 20;

const QString kUnorderedListFormat = QStringLiteral("无序列表");
const QString kOrderedListFormat = QStringLiteral("有序列表");
const QString kHeadingFormat = QStringLiteral("标题");
const QString kModifierPlaceholder = QStringLiteral("…");
const QString kOrderedModifierPlaceholder = QStringLiteral("意指");
const QString kSelfPlaceholder = QStringLiteral("self");
const QString kRelationPlaceholder = QStringLiteral("{}'"); // 关系段可省时的注入提示
const QString kDefaultTheme = QStringLiteral("Universe");
const QString kSeparatorNarrow = QStringLiteral(":");
const QString kSeparatorWide = QStringLiteral(": ");
const QString kSeparatorRelation = QStringLiteral(" ⇒ ");

// 空输入框回退到占位文本宽度：占位文本即用户应填内容，空框也应完整可见
void fitWidthToPlaceholder(QLineEdit *lineEdit)
{
    if(!lineEdit->text().isEmpty())
        return;
    const int width = lineEdit->fontMetrics().horizontalAdvance(lineEdit->placeholderText()) + kTextSidePadding;
    lineEdit->setFixedWidth(width);
}

void fitWidthToContent(QLineEdit *lineEdit, const QString &text)
{
    const QFontMetrics metrics = lineEdit->fontMetrics();
    const int placeholderWidth = metrics.horizontalAdvance(lineEdit->placeholderText()) + kTextSidePadding;
    const int contentWidth = metrics.horizontalAdvance(text) + kTextSidePadding;
    lineEdit->setFixedWidth(qMax(placeholderWidth, contentWidth));
}

// 依缺段裁剪分隔符，拼出「修饰: 中心 ⇒ 关系: 注入」文本
QString composeGeneralDescription(const QString &fText, const QString &xText,
                                  const QString &rText, const QString &mText)
{
    QString separatorAfterModifier = kSeparatorWide;
    QString separatorAfterCenter = kSeparatorRelation;
    QString separatorAfterRelation = kSeparatorWide;

    if(xText.isEmpty())
        separatorAfterModifier = kSeparatorNarrow;
    if(fText.isEmpty())
    {
        separatorAfterModifier = QString();
        if(xText.isEmpty() && !rText.isEmpty())
            separatorAfterCenter = QString();
    }
    if(rText.isEmpty())
    {
        separatorAfterRelation = QString();
        if(mText.isEmpty())
            separatorAfterCenter = QString();
    }

    return fText + separatorAfterModifier + xText + separatorAfterCenter + rText
           + separatorAfterRelation + mText;
}

// 拼接描述文本；四条段全空、仅修饰段、修饰+中心这些特例需保留原分隔符方案
QString composeDescriptionContent(const QString &fText, const QString &xText,
                                  const QString &rText, const QString &mText)
{
    const bool fEmpty = fText.isEmpty();
    const bool xEmpty = xText.isEmpty();
    const bool rEmpty = rText.isEmpty();
    const bool mEmpty = mText.isEmpty();
    const bool modifierCenterOnly = !fEmpty && !xEmpty && rEmpty && mEmpty;

    if(fEmpty && xEmpty && rEmpty && mEmpty) // 全空：仅去掉修饰段分隔符
        return kSeparatorRelation + kSeparatorWide;
    if(!fEmpty && xEmpty && rEmpty && mEmpty) // 仅修饰段：窄冒号
        return fText + kSeparatorNarrow + kSeparatorRelation + kSeparatorWide;
    if(modifierCenterOnly) // 修饰+中心：全部默认分隔符
        return fText + kSeparatorWide + xText + kSeparatorRelation + kSeparatorWide;
    return composeGeneralDescription(fText, xText, rText, mText);
}
}

DescAddDialog::DescAddDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::DescAddDialog)
{
    ui->setupUi(this);
    adjustSizeByPlaceholder();
}

DescAddDialog::~DescAddDialog()
{
    delete ui;
}

// 空框按占位文本宽度对齐，避免默认提示被截断
void DescAddDialog::adjustSizeByPlaceholder()
{
    fitWidthToPlaceholder(ui->fLineEdit);
    fitWidthToPlaceholder(ui->xLineEdit);
    fitWidthToPlaceholder(ui->rLineEdit);
    fitWidthToPlaceholder(ui->mLineEdit);
}

void DescAddDialog::updatePlaceholderTexts()
{
    const QString xText = ui->xLineEdit->text();
    const QString rText = ui->rLineEdit->text();
    const QString mText = ui->mLineEdit->text();
    ui->mLineEdit->setPlaceholderText(kSelfPlaceholder);
    if(mText.isEmpty() && (xText.isEmpty() || !rText.isEmpty())) // 关系段空且中心段可省时改用注入提示
        ui->mLineEdit->setPlaceholderText(kRelationPlaceholder);
    adjustSizeByPlaceholder();
}

void DescAddDialog::reset()
{
    QLineEdit *lineEdits[] = {ui->fLineEdit, ui->xLineEdit, ui->rLineEdit, ui->mLineEdit};
    for(QLineEdit *lineEdit : lineEdits)
    {
        lineEdit->setEnabled(true);
        lineEdit->setText(QString());
    }
    ui->fLineEdit->setPlaceholderText(kModifierPlaceholder);
    applyFormatState();
    adjustSizeByPlaceholder();
    ui->xLineEdit->setFocus();

    ui->horizontalSpacer->changeSize(0, 0);
    adjustSize();
}

// 依格式施加禁用/占位差异（标题禁关系段、有序列表给出修饰段提示）
void DescAddDialog::applyFormatState()
{
    const QString formatName = ui->comboBox_2->currentText();
    if(formatName == kHeadingFormat)
    {
        ui->rLineEdit->setDisabled(true);
        ui->mLineEdit->setDisabled(true);
    }
    else if(formatName == kOrderedListFormat)
        ui->fLineEdit->setPlaceholderText(kOrderedModifierPlaceholder);
}

void DescAddDialog::on_fLineEdit_textChanged(const QString &text)
{
    fitWidthToContent(ui->fLineEdit, text);
    updatePlaceholderTexts();
    adjustSize();
}

void DescAddDialog::on_xLineEdit_textChanged(const QString &text)
{
    fitWidthToContent(ui->xLineEdit, text);
    updatePlaceholderTexts();
    adjustSize();
}

void DescAddDialog::on_rLineEdit_textChanged(const QString &text)
{
    fitWidthToContent(ui->rLineEdit, text);
    updatePlaceholderTexts();
    adjustSize();
}

void DescAddDialog::on_mLineEdit_textChanged(const QString &text)
{
    fitWidthToContent(ui->mLineEdit, text);
    updatePlaceholderTexts();
    adjustSize();
}

QString DescAddDialog::getRetType() const
{
    return retType;
}

QString DescAddDialog::getRetContent() const
{
    return retContent;
}

void DescAddDialog::setThemeBeing(const QString &themeBeing)
{
    if(themeBeing.isEmpty())
        ui->themeBeingLabel->setText(kDefaultTheme);
    else
        ui->themeBeingLabel->setText(themeBeing);
}

// 切换格式时先重置再按新格式回填原内容（标题丢弃关系段、有序列表丢弃修饰段）
void DescAddDialog::on_comboBox_2_currentTextChanged(const QString &formatName)
{
    const QString fText = ui->fLineEdit->text();
    const QString xText = ui->xLineEdit->text();
    const QString rText = ui->rLineEdit->text();
    const QString mText = ui->mLineEdit->text();
    reset();

    if(formatName == kHeadingFormat)
    {
        ui->fLineEdit->setText(fText);
        ui->xLineEdit->setText(xText);
    }
    else if(formatName == kUnorderedListFormat)
    {
        ui->fLineEdit->setText(fText);
        ui->xLineEdit->setText(xText);
        ui->rLineEdit->setText(rText);
        ui->mLineEdit->setText(mText);
    }
    else if(formatName == kOrderedListFormat)
    {
        ui->xLineEdit->setText(xText);
        ui->rLineEdit->setText(rText);
        ui->mLineEdit->setText(mText);
    }
}

void DescAddDialog::on_buttonBox_accepted()
{
    retContent = composeDescriptionContent(ui->fLineEdit->text(), ui->xLineEdit->text(),
                                           ui->rLineEdit->text(), ui->mLineEdit->text());
    retType = ui->comboBox_2->currentText();
}