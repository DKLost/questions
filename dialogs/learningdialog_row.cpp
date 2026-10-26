#include "learningdialog.h"
#include "ui_learningdialog.h"
#include "learningdialog_internal.h"
#include "utils/toolfunctions.h"
#include "core/fsrs.h"
#include "core/knowledgedb.h"
#include "widgets/knowledgetreehelper.h"
#include "widgets/questiontableviewhelper.h"
#include <QSet>
#include <QImage>
#include <QUrl>
#include <QPainter>
#include <QTextImageFormat>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <QSqlError>
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
constexpr int kComboBoxWidthPadding = 50;
constexpr int kComboBoxHeightPadding = 10;
constexpr int kComboBoxHeight = 28;

const QString kAccuracyText = QStringLiteral("正确率(%1\%):%2/%3");

const QString kTypstSourceFile = QStringLiteral("temp.typ");
const QString kTypstPreviewFile = QStringLiteral("temp.png");
const QString kTypstStyleMarker = QStringLiteral("green"); // typst 输入行的边框样式标记
}

// 输入行：自适应宽度；typst 行同时刷新公式预览
void LearningDialog::onAnswerInputChanged(const QString &text)
{
    currentLineEdit = qobject_cast<QLineEdit *>(sender());
    if(!currentLineEdit)
        return;
    const int textWidth = currentLineEdit->fontMetrics().horizontalAdvance(text) + kInputWidthPadding;
    currentLineEdit->setFixedWidth(qMax(textWidth, currentLineEdit->statusTip().toInt())); // statusTip 记录初始宽度

    if(!currentLineEdit->styleSheet().contains(kTypstStyleMarker)) // 只有 typst 输入行需要公式预览
    {
        ToolFunctions::watch_typst_stop(typstWatchProcess);
        return;
    }
    if(typstWatchProcess.state() == QProcess::NotRunning)
        ToolFunctions::watch_typst_start(typstWatchProcess, kTypstSourceFile, kTypstPreviewFile);
    ToolFunctions::write_file(kTypstSourceFile,
                              ToolFunctions::typstMathPrefix + QStringLiteral("$ %1 $").arg(currentLineEdit->text()));
}

// 乱序池选框：自适应大小 + 同池去重（一个池内每个答案只能被一个选框占用）
void LearningDialog::onPoolAnswerChanged(int poolIndex)
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(sender());
    if(!comboBox)
        return;
    resizePoolComboBox(comboBox, poolIndex);
    reassignPoolSelections(comboBox, poolIndex);
}

// 有图标按图标实际大小自适应，否则按文本宽度
void LearningDialog::resizePoolComboBox(QComboBox *comboBox, int poolIndex)
{
    const QIcon icon = comboBox->itemIcon(poolIndex);
    if(icon.isNull())
    {
        const int width = kComboBoxWidthPadding
                          + comboBox->fontMetrics().horizontalAdvance(comboBox->itemText(poolIndex));
        comboBox->setFixedSize(width, kComboBoxHeight);
        return;
    }
    QSize size = icon.actualSize(kIconLookupSize);
    size.setWidth(kComboBoxWidthPadding + size.width());
    size.setHeight(kComboBoxHeightPadding + size.height());
    comboBox->setFixedSize(size);
    comboBox->setIconSize(icon.actualSize(kIconLookupSize));
    comboBox->view()->setIconSize(kIconLookupSize);
}

// 同池去重：把"被两个选框同时选中"的答案让给空闲答案（其它选框换成池内还没被占用的行号）
void LearningDialog::reassignPoolSelections(QComboBox *changedComboBox, int poolIndex)
{
    if(!changedComboBox->itemData(poolIndex).isValid())
        return;
    const int selectedAnswerRow = changedComboBox->itemData(poolIndex).toInt();
    const QJsonObject selectedAnswer = currentAnswers[selectedAnswerRow].toObject();
    const int pool = selectedAnswer.value(AnswerStore::FieldPool).toInt();
    changedComboBox->setToolTip(selectedAnswer.value(AnswerStore::FieldContent).toString());

    QMap<int, int> ownerOfAnswerRow; // 池内答案原行号 → 持有它的选框行号（-1 = 空闲）
    for(int i = 0; i < changedComboBox->count(); ++i)
        ownerOfAnswerRow[changedComboBox->itemData(i).toInt()] = -1;

    int changedComboBoxRow = selectedAnswerRow; // 选框尚未加入布局时（创建期）它就是本行
    for(int row = 0; row < answerRowCount; ++row)
    {
        QComboBox *comboBox = poolComboBoxAt(row, pool);
        if(!comboBox)
            continue;
        if(comboBox == changedComboBox)
            changedComboBoxRow = row;
        ownerOfAnswerRow[comboBox->itemData(comboBox->currentIndex()).toInt()] = row;
    }
    ownerOfAnswerRow[selectedAnswerRow] = changedComboBoxRow;

    for(int row = 0; row < answerRowCount; ++row)
    {
        QComboBox *comboBox = poolComboBoxAt(row, pool);
        if(!comboBox || comboBox == changedComboBox)
            continue;
        const int comboBoxAnswerRow = comboBox->itemData(comboBox->currentIndex()).toInt();
        if(ownerOfAnswerRow.value(comboBoxAnswerRow, -1) == row)
            continue; // 无冲突
        for(int freeAnswerRow : ownerOfAnswerRow.keys())
            if(ownerOfAnswerRow.value(freeAnswerRow, 0) == -1)
            {
                ownerOfAnswerRow[freeAnswerRow] = row;
                comboBox->setCurrentIndex(comboBox->findData(freeAnswerRow));
                break;
            }
    }
}

// 取 row 行上属于指定池的乱序选框；该行不是同池选框时返回空
QComboBox *LearningDialog::poolComboBoxAt(int row, int pool) const
{
    QComboBox *comboBox = answerComboAt(row);
    if(!comboBox || currentAnswers[row].toObject().value(AnswerStore::FieldPool).toInt() != pool)
        return nullptr;
    return comboBox;
}

void LearningDialog::onTypstWatcher_standard_output()
{
    if(!currentLineEdit)
        return;
    QLabel *previewLabel = answerRowAt(currentLineEdit->toolTip().toInt()).typstPreview;
    if(previewLabel)
        previewLabel->setPixmap(QPixmap{kTypstPreviewFile});
}

void LearningDialog::updateAccuracyLabel(int batchCorrect, int batchTotal)
{
    totalCount += batchTotal; // 跨组累计
    correctCount += batchCorrect;
    const QString accuracy = QString::number(double(correctCount) / totalCount * 100, 'f', 2);
    ui->accuracyLabel->setText(kAccuracyText.arg(accuracy).arg(correctCount).arg(totalCount));
}