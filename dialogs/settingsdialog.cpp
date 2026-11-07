#include "dialogs/settingsdialog.h"
#include "core/fsrs.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const AppSettings &settings, QWidget *parent)
    : QDialog(parent), current(settings)
{
    setWindowTitle(QStringLiteral("设置"));
    resize(600, 520);

    auto *tabs = new QTabWidget(this);

    // 学习算法
    auto *learnTab = new QWidget(tabs);
    auto *learnLayout = new QVBoxLayout(learnTab);
    auto *learnForm = new QFormLayout;

    weightsEdit = new QPlainTextEdit(learnTab);
    weightsEdit->setPlainText(AppSettings::weightsToText(current.weights));
    weightsEdit->setPlaceholderText(QStringLiteral("21 个数字，用逗号、空格或换行分隔（fsrs-optimizer 输出的 w[0]…w[20]）"));
    weightsEdit->setMinimumHeight(110);
    learnForm->addRow(QStringLiteral("FSRS 权重:"), weightsEdit);

    auto *resetButton = new QPushButton(QStringLiteral("恢复默认权重"), learnTab);
    connect(resetButton, &QPushButton::clicked, this, &SettingsDialog::resetWeightsToDefault);
    learnForm->addRow(QString(), resetButton);

    auto *weightsHint = new QLabel(QStringLiteral("修改只影响之后的复习排期，不会重算已排好的到期日。"), learnTab);
    weightsHint->setWordWrap(true);
    learnForm->addRow(weightsHint);

    retentionSpin = new QDoubleSpinBox(learnTab);
    retentionSpin->setRange(0.70, 0.97);
    retentionSpin->setDecimals(2);
    retentionSpin->setSingleStep(0.01);
    retentionSpin->setValue(current.requestRetention);
    learnForm->addRow(QStringLiteral("目标保留率:"), retentionSpin);

    maxIntervalSpin = new QSpinBox(learnTab);
    maxIntervalSpin->setRange(1, 36500);
    maxIntervalSpin->setSuffix(QStringLiteral(" 天"));
    maxIntervalSpin->setValue(current.maximumInterval);
    learnForm->addRow(QStringLiteral("最大间隔:"), maxIntervalSpin);

    easyFactorSpin = new QDoubleSpinBox(learnTab);
    easyFactorSpin->setRange(0.10, 1.00);
    easyFactorSpin->setDecimals(2);
    easyFactorSpin->setSingleStep(0.05);
    easyFactorSpin->setValue(current.easyFactor);
    learnForm->addRow(QStringLiteral("easy 系数:"), easyFactorSpin);

    hardFactorSpin = new QDoubleSpinBox(learnTab);
    hardFactorSpin->setRange(0.50, 3.00);
    hardFactorSpin->setDecimals(2);
    hardFactorSpin->setSingleStep(0.05);
    hardFactorSpin->setValue(current.hardFactor);
    learnForm->addRow(QStringLiteral("hard 系数:"), hardFactorSpin);

    newCardFactorSpin = new QDoubleSpinBox(learnTab);
    newCardFactorSpin->setRange(1.0, 5.0);
    newCardFactorSpin->setDecimals(1);
    newCardFactorSpin->setSingleStep(0.1);
    newCardFactorSpin->setValue(current.newCardTimeFactor);
    learnForm->addRow(QStringLiteral("新卡估时系数:"), newCardFactorSpin);

    auto *ratingHint = new QLabel(
        QStringLiteral("评级规则：用时<良好用时×easy系数 → easy；\n\t用时>良好用时×hard系数 → hard；\n\t其余为 good。"
                       "\n\t新卡估时：首次作答用时×新卡估时系数=初始良好用时。"),
        learnTab);
    ratingHint->setWordWrap(true);
    learnForm->addRow(ratingHint);

    learnLayout->addLayout(learnForm);
    learnLayout->addStretch();
    tabs->addTab(learnTab, QStringLiteral("学习算法"));

    // 环境
    auto *envTab = new QWidget(tabs);
    auto *envForm = new QFormLayout(envTab);
    auto *typstRow = new QWidget(envTab);
    auto *typstLayout = new QHBoxLayout(typstRow);
    typstLayout->setContentsMargins(0, 0, 0, 0);
    typstEdit = new QLineEdit(current.typstCommand, typstRow);
    auto *browseButton = new QPushButton(QStringLiteral("浏览…"), typstRow);
    typstLayout->addWidget(typstEdit);
    typstLayout->addWidget(browseButton);
    envForm->addRow(QStringLiteral("typst 可执行文件:"), typstRow);
    auto *envHint = new QLabel(QStringLiteral("填 typst 表示从 PATH 或运行目录查找；修改后新编译生效。"), envTab);
    envHint->setWordWrap(true);
    envForm->addRow(envHint);
    connect(browseButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择 typst 可执行文件"),
                                                          typstEdit->text(),
                                                          QStringLiteral("typst (typst.exe typst);;所有文件 (*)"));
        if(!path.isEmpty())
            typstEdit->setText(QDir::toNativeSeparators(path));
    });
    tabs->addTab(envTab, QStringLiteral("环境"));

    // 数据
    auto *dataTab = new QWidget(tabs);
    auto *dataLayout = new QVBoxLayout(dataTab);
    backupCheck = new QCheckBox(QStringLiteral("导入笔记前自动备份数据库"), dataTab);
    backupCheck->setChecked(current.backupBeforeImport);
    dataLayout->addWidget(backupCheck);

    auto *keepRow = new QWidget(dataTab);
    auto *keepLayout = new QHBoxLayout(keepRow);
    keepLayout->setContentsMargins(0, 0, 0, 0);
    keepLayout->addWidget(new QLabel(QStringLiteral("保留最近"), keepRow));
    backupKeepSpin = new QSpinBox(keepRow);
    backupKeepSpin->setRange(1, 50);
    backupKeepSpin->setSuffix(QStringLiteral(" 份"));
    backupKeepSpin->setValue(current.backupKeepCount);
    backupKeepSpin->setEnabled(current.backupBeforeImport);
    keepLayout->addWidget(backupKeepSpin);
    keepLayout->addStretch();
    dataLayout->addWidget(keepRow);
    connect(backupCheck, &QCheckBox::toggled, this, [this](bool checked) { backupKeepSpin->setEnabled(checked); });

    auto *dataHint =
        new QLabel(QStringLiteral("备份位置：./backup/<时间戳>/，包含 question.db 与 knowledge.db。"
                                  "parser 导入会先清空知识库，关闭备份后该操作不可恢复。"),
                   dataTab);
    dataHint->setWordWrap(true);
    dataLayout->addWidget(dataHint);
    dataLayout->addStretch();
    tabs->addTab(dataTab, QStringLiteral("数据"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addWidget(tabs);
    root->addWidget(buttons);
}

AppSettings SettingsDialog::settings() const
{
    return current;
}

void SettingsDialog::accept()
{
    QList<double> weights;
    if(!readWeights(&weights))
    {
        QMessageBox::warning(this, QStringLiteral("设置"),
                             QStringLiteral("FSRS 权重必须是 21 个非负数字，且第 21 个（w[20]）必须大于 0。"));
        return;
    }
    if(easyFactorSpin->value() >= hardFactorSpin->value())
    {
        QMessageBox::warning(this, QStringLiteral("设置"),
                             QStringLiteral("easy 系数必须小于 hard 系数。"));
        return;
    }

    current.weights = weights;
    current.requestRetention = retentionSpin->value();
    current.maximumInterval = maxIntervalSpin->value();
    current.easyFactor = easyFactorSpin->value();
    current.hardFactor = hardFactorSpin->value();
    current.newCardTimeFactor = newCardFactorSpin->value();
    current.typstCommand = typstEdit->text().trimmed();
    if(current.typstCommand.isEmpty())
        current.typstCommand = QStringLiteral("typst");
    current.backupBeforeImport = backupCheck->isChecked();
    current.backupKeepCount = backupKeepSpin->value();

    QDialog::accept();
}

void SettingsDialog::resetWeightsToDefault()
{
    QList<double> defaults;
    defaults.reserve(21);
    for(int i = 0; i < 21; ++i)
        defaults.append(FSRS::defaultW[i]);
    weightsEdit->setPlainText(AppSettings::weightsToText(defaults));
}

bool SettingsDialog::readWeights(QList<double> *weights) const
{
    bool ok = false;
    const QList<double> parsed = AppSettings::parseWeights(weightsEdit->toPlainText(), &ok);
    if(ok && weights)
        *weights = parsed;
    return ok;
}
