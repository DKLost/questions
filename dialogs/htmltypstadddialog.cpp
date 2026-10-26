#include "htmltypstadddialog.h"
#include "ui_htmltypstadddialog.h"
#include "utils/toolfunctions.h"

namespace
{
constexpr int kMinLineEditWidth = 200;
constexpr int kLineEditTextPadding = 35;
constexpr int kFixedWidthMargin = 30;
constexpr int kHashHexLength = 32;
const QString kTempTypstPath = QStringLiteral("temp.typ");
const QString kTempPngPath = QStringLiteral("temp.png");
const QString kMathOpen = QStringLiteral("$ ");
const QString kMathClose = QStringLiteral(" $");
const QString kPngPathFormat = QStringLiteral("./data/%1/%2.png");
const QString kTypstSourcePathFormat = QStringLiteral("./data/%1/%2.typmath");
}

HtmlTypstAddDialog::HtmlTypstAddDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::HtmlTypstAddDialog)
{
    ui->setupUi(this);
    typstWatchProcess.setProcessChannelMode(QProcess::MergedChannels);
    connect(&typstWatchProcess, &QProcess::readyReadStandardOutput, this, &HtmlTypstAddDialog::onTypstWatcher_standard_output);
}

HtmlTypstAddDialog::~HtmlTypstAddDialog()
{
    delete ui;
}

// 停止仍在运行的预览进程（重复调用安全）
void HtmlTypstAddDialog::stopWatcher()
{
    if(typstWatchProcess.state() != QProcess::NotRunning)
        ToolFunctions::watch_typst_stop(typstWatchProcess);
}

void HtmlTypstAddDialog::startWatcherIfNeeded()
{
    if(typstWatchProcess.state() == QProcess::NotRunning)
        ToolFunctions::watch_typst_start(typstWatchProcess, kTempTypstPath, kTempPngPath);
}

// 以内容 sha256 前若干位作文件名，随题目 data 目录归档
QString HtmlTypstAddDialog::contentHash() const
{
    return ToolFunctions::shortHash(ui->lineEdit->text(), kHashHexLength);
}

void HtmlTypstAddDialog::reset()
{
    ui->lineEdit->clear();
    ui->label->clear();
    retPath.clear();
    stopWatcher();
}

void HtmlTypstAddDialog::on_lineEdit_textChanged(const QString &text)
{
    int pixelWidth = ui->lineEdit->fontMetrics().horizontalAdvance(text) + kLineEditTextPadding;
    pixelWidth = qMax(pixelWidth, kMinLineEditWidth);
    setFixedWidth(pixelWidth + kFixedWidthMargin);

    const QString typstSource = ToolFunctions::typstMathPrefix + kMathOpen + text + kMathClose;
    startWatcherIfNeeded();
    ToolFunctions::write_file(kTempTypstPath, typstSource); // 写 temp.typ，预览 temp.png 由 watcher 回显
}

void HtmlTypstAddDialog::on_buttonBox_accepted()
{
    const QString hash = contentHash();
    stopWatcher();
    retPath = kPngPathFormat.arg(questionId).arg(hash);
    ToolFunctions::write_file(kTypstSourcePathFormat.arg(questionId).arg(hash), ui->lineEdit->text()); // 同步保留 typst 源码，便于二次编辑
    ToolFunctions::compile_typst(kTempTypstPath, retPath);
}

void HtmlTypstAddDialog::onTypstWatcher_standard_output()
{
    ui->label->setPixmap(QPixmap{kTempPngPath});
}

QString HtmlTypstAddDialog::getRetPath() const
{
    return retPath;
}

void HtmlTypstAddDialog::setText(const QString &text)
{
    ui->lineEdit->setText(text);
}

void HtmlTypstAddDialog::setQId(int newQId)
{
    questionId = newQId;
}