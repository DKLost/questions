#include "answereditdialog.h"
#include "ui_answereditdialog.h"
#include "bindanswerdialog.h"
#include "services/answerstore.h"
#include "utils/toolfunctions.h"

#include <QDir>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QMessageBox>
#include <QPixmap>

namespace {
const QString kTypeAuto = QStringLiteral("auto");
const QString kTypeManual = QStringLiteral("manual");
const QString kTypeManualImage = QStringLiteral("manual(image)");
const QString kTypeTypst = QStringLiteral("auto(typst)");
const QString kTypeChain = QStringLiteral("auto(chain)");
const QString kTypeChainTypst = QStringLiteral("auto(chain-typst)");

const QString kDisplayAuto = QStringLiteral("自动检查");
const QString kDisplayManual = QStringLiteral("手动检查");
const QString kDisplayManualImage = QStringLiteral("手动检查(图片)");
const QString kDisplayTypst = QStringLiteral("自动检查(typst)");
const QString kDisplayChain = QStringLiteral("链卡(按图谱判分)");
const QString kDisplayChainTypst = QStringLiteral("链卡(typst,按图谱判分)");

const QMap<QString, QString> kTypeByDisplayName{{kDisplayAuto, kTypeAuto},
                                                {kDisplayManual, kTypeManual},
                                                {kDisplayManualImage, kTypeManualImage},
                                                {kDisplayTypst, kTypeTypst},
                                                {kDisplayChain, kTypeChain},
                                                {kDisplayChainTypst, kTypeChainTypst}};
const QMap<QString, QString> kDisplayNameByType{{kTypeAuto, kDisplayAuto},
                                                {kTypeManual, kDisplayManual},
                                                {kTypeManualImage, kDisplayManualImage},
                                                {kTypeTypst, kDisplayTypst},
                                                {kTypeChain, kDisplayChain},
                                                {kTypeChainTypst, kDisplayChainTypst}};

const QString kJsonFieldType = QStringLiteral("type");
const QString kJsonFieldId = QStringLiteral("id");

const QString kTypstTempInput = QStringLiteral("temp.typ");
const QString kTypstTempOutput = QStringLiteral("temp.png");
const QString kQuestionImageDirFormat = QStringLiteral("%1/data/%2/");
const QString kAnswerImagePathFormat = QStringLiteral("./data/%1/%2.png");
const QString kDuplicateChainTitle = QStringLiteral("链卡重复");
const QString kDuplicateChainMessage = QStringLiteral("每题至多允许一个链卡（按图谱判分），请先删除或修改已有的链卡。");

const int kInvalidAnswerId = -1;
const int kTextWidthPadding = 20;
const int kLineEditMinWidth = 180;
const int kDialogWidthPadding = 50;
const int kPreviewImagePadding = 20;
const int kChainPoolDisabled = 0;
} // namespace

AnswerEditDialog::AnswerEditDialog(BindAnswerDialog *mainBindAnswerDialog, QWidget *parent)
    : QDialog(parent), ui(new Ui::AnswerEditDialog)
{
    ui->setupUi(this);
    bindAnswerDialog = mainBindAnswerDialog;
    questionId = kInvalidAnswerId; // 未 setQId 前视为无题，避免读到未初始化 id
    retAId = kInvalidAnswerId;
    retPool = 0;
    retInjectBId = kInvalidAnswerId;
    ui->displayLabel->hide();
    typstWatchProcess.setProcessChannelMode(QProcess::MergedChannels);
    connect(&typstWatchProcess, &QProcess::readyReadStandardOutput, this, &AnswerEditDialog::onTypstWatcher_standard_output);
}

AnswerEditDialog::~AnswerEditDialog()
{
    delete ui;
}

void AnswerEditDialog::resetEdit()
{
    ui->lineEdit->clear();
    ui->lineEdit->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->poolComboBox->setEnabled(true);
    retContent = "";
    ui->comboBox->setCurrentText(kDisplayAuto);
    retType = kTypeAuto;
}

void AnswerEditDialog::setQId(int newQId)
{
    questionId = newQId;
}

void AnswerEditDialog::lineEdit_selectAll()
{
    ui->lineEdit->setFocus();
    ui->lineEdit->selectAll();
}

void AnswerEditDialog::on_pushButton_clicked()
{
    if(ui->comboBox->currentText() != kDisplayManualImage)
        return;
    const QString dirPath = kQuestionImageDirFormat.arg(QDir::currentPath()).arg(questionId);
    const QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("打开图片"), dirPath, tr("Image Files (*.jpg *.png *.gif)"));
    if(filePath.isEmpty())
        return;
    ui->lineEdit->setText(QDir{}.relativeFilePath(filePath)); // 题内图片存相对路径，随 data 目录整体搬移
}

void AnswerEditDialog::setType(const QString &type)
{
    ui->comboBox->setCurrentText(kDisplayNameByType.value(type));
}
void AnswerEditDialog::setContent(const QString &content)
{
    if(AnswerStore::isChainCardType(retType))
        return; // 链卡标准答案由挂载 DerivedAbs 裁决，不保留任何手填内容
    ui->lineEdit->setText(content);
}
void AnswerEditDialog::setGoodTime(const QTime &goodTime)
{
    ui->goodTimeEdit->setTime(goodTime);
}
void AnswerEditDialog::setAId(const int &aId)
{
    ui->bindIdLineEdit->setText(QString::number(aId));
}

void AnswerEditDialog::setPool(const int &pool)
{
    ui->poolComboBox->setCurrentText(QString::number(pool));
}

QTime AnswerEditDialog::getRetGoodTime() const
{
    return retGoodTime;
}
QString AnswerEditDialog::getRetContent() const
{
    return retContent;
}
QString AnswerEditDialog::getRetType() const
{
    return retType;
}
int AnswerEditDialog::getRetAId() const
{
    return retAId;
}
int AnswerEditDialog::getRetPool() const
{
    return retPool;
}

void AnswerEditDialog::setSiblingAnswers(const QJsonArray &answers)
{
    siblingAnswers = answers;
}

void AnswerEditDialog::on_comboBox_currentTextChanged(const QString &displayName)
{
    Q_UNUSED(displayName);
    retType = kTypeByDisplayName.value(ui->comboBox->currentText());
    if(retType == kTypeTypst)
        applyTypstAnswerMode();
    else if(AnswerStore::isChainCardType(retType))
        applyChainAnswerMode();
    else
        applyDefaultAnswerMode();
}

void AnswerEditDialog::applyTypstAnswerMode()
{
    ui->lineEdit->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->displayLabel->show();
    on_lineEdit_textChanged(ui->lineEdit->text()); // 切到 typst 立即刷新预览与宽度
}

// 链卡答案由挂载 DerivedAbs 的末段决定，不用手填标准答案，也不参与乱序池
void AnswerEditDialog::applyChainAnswerMode()
{
    stopTypstPreview();
    ui->displayLabel->hide();
    ui->lineEdit->clear();
    ui->lineEdit->setEnabled(false);
    ui->pushButton->setEnabled(false);
    ui->poolComboBox->setEnabled(false);
    ui->poolComboBox->setCurrentText(QString::number(kChainPoolDisabled));
    retPool = kChainPoolDisabled;
    retContent = "";
}

void AnswerEditDialog::applyDefaultAnswerMode()
{
    ui->lineEdit->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->displayLabel->hide();
}

void AnswerEditDialog::stopTypstPreview()
{
    if(typstWatchProcess.state() != QProcess::NotRunning)
        ToolFunctions::watch_typst_stop(typstWatchProcess);
}

void AnswerEditDialog::on_goodTimeEdit_userTimeChanged(const QTime &time)
{
    retGoodTime = time;
}

void AnswerEditDialog::updateLineEditWidth(const QString &text)
{
    const int textWidth = qMax(ui->lineEdit->fontMetrics().horizontalAdvance(text) + kTextWidthPadding, kLineEditMinWidth);
    const int imageWidth = ui->displayLabel->isHidden() ? 0 : ui->displayLabel->pixmap().width();
    ui->lineEdit->setFixedWidth(textWidth);
    setFixedWidth(qMax(imageWidth, textWidth) + kDialogWidthPadding);
}

void AnswerEditDialog::on_lineEdit_textChanged(const QString &text)
{
    updateLineEditWidth(text);

    if(AnswerStore::isChainCardType(retType))
    {
        retContent = ""; // 禁用态下 clear 仍会触发本槽，链卡内容必须恒空
        return;
    }
    retContent = ui->lineEdit->text();
    updateTypstPreview();
}

void AnswerEditDialog::updateTypstPreview()
{
    if(kTypeByDisplayName.value(ui->comboBox->currentText()) != kTypeTypst)
    {
        stopTypstPreview();
        return;
    }
    const QString typstSource = ToolFunctions::typstMathPrefix + "$ " + ui->lineEdit->text() + " $";
    if(typstWatchProcess.state() == QProcess::NotRunning)
        ToolFunctions::watch_typst_start(typstWatchProcess, kTypstTempInput, kTypstTempOutput);
    ToolFunctions::write_file(kTypstTempInput, typstSource); // 写 temp.typ，预览 temp.png 由 watcher 回显
}

void AnswerEditDialog::refreshTypstPreviewImage()
{
    ui->displayLabel->setPixmap(QPixmap{kTypstTempOutput});
    ui->displayLabel->setFixedHeight(ui->displayLabel->pixmap().height());
    ui->displayLabel->setFixedWidth(ui->displayLabel->pixmap().width());
}

void AnswerEditDialog::onTypstWatcher_standard_output()
{
    refreshTypstPreviewImage();
    const int textWidth = qMax(ui->lineEdit->fontMetrics().horizontalAdvance(ui->lineEdit->text()) + kTextWidthPadding, kLineEditMinWidth);
    const int imageWidth = ui->displayLabel->pixmap().width();
    ui->lineEdit->setFixedWidth(textWidth);
    setFixedWidth(qMax(imageWidth + kPreviewImagePadding, textWidth + kDialogWidthPadding));
}

void AnswerEditDialog::on_bindIdLineEdit_textChanged(const QString &text)
{
    retAId = text.toInt();
}
void AnswerEditDialog::on_poolComboBox_currentTextChanged(const QString &text)
{
    retPool = text.toInt();
}

void AnswerEditDialog::on_injectBindButton_clicked()
{
    const int newAId = pickBoundAnswerId();
    if(newAId != kInvalidAnswerId && newAId != retAId)
        ui->injectLineEdit->setText(QString::number(newAId));
}
int AnswerEditDialog::getRetInjectBId() const
{
    return retInjectBId;
}
void AnswerEditDialog::setInjectBId(int injectBId)
{
    ui->injectLineEdit->setText(QString::number(injectBId));
}
void AnswerEditDialog::on_injectLineEdit_textChanged(const QString &text)
{
    retInjectBId = text.toInt();
}

void AnswerEditDialog::on_bindButton_clicked()
{
    const int newAId = pickBoundAnswerId();
    if(newAId != kInvalidAnswerId && newAId != retAId)
        ui->bindIdLineEdit->setText(QString::number(newAId));
}

int AnswerEditDialog::pickBoundAnswerId()
{
    bindAnswerDialog->initDialog(questionId, retAId);
    bindAnswerDialog->exec();
    return bindAnswerDialog->getRetAId();
}

void AnswerEditDialog::on_buttonBox_accepted()
{
    stopTypstPreview();
    if(ui->comboBox->currentText() == kDisplayTypst)
        ToolFunctions::compile_typst(kTypstTempInput, kAnswerImagePathFormat.arg(questionId).arg(retAId));
}

bool AnswerEditDialog::hasOtherChainCard() const
{
    for(const QJsonValue &answerValue : siblingAnswers)
    {
        const QJsonObject answerObject = answerValue.toObject();
        if(AnswerStore::isChainCardType(answerObject.value(kJsonFieldType).toString()) && answerObject.value(kJsonFieldId).toInt() != retAId)
            return true;
    }
    return false;
}

void AnswerEditDialog::accept()
{
    if(AnswerStore::isChainCardType(retType)) // 每题至多一个链卡（排除自身 aid）
    {
        if(hasOtherChainCard())
        {
            QMessageBox::warning(this, kDuplicateChainTitle, kDuplicateChainMessage);
            return; // 不调 accept，窗口保持打开
        }
        retContent = "";
    }
    QDialog::accept();
}