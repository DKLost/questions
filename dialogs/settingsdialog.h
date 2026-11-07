#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

#include "services/appsettings.h"

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

// 设置：学习算法（FSRS 权重/保留率/间隔/评级阈值）、环境（typst 路径）、数据（导入前备份）
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(const AppSettings &settings, QWidget *parent = nullptr);
    AppSettings settings() const;

protected:
    void accept() override;

private:
    void resetWeightsToDefault();
    bool readWeights(QList<double> *weights) const;

    AppSettings current;
    QPlainTextEdit *weightsEdit = nullptr;
    QDoubleSpinBox *retentionSpin = nullptr;
    QSpinBox *maxIntervalSpin = nullptr;
    QDoubleSpinBox *easyFactorSpin = nullptr;
    QDoubleSpinBox *hardFactorSpin = nullptr;
    QDoubleSpinBox *newCardFactorSpin = nullptr;
    QLineEdit *typstEdit = nullptr;
    QCheckBox *backupCheck = nullptr;
    QSpinBox *backupKeepSpin = nullptr;
};

#endif // SETTINGSDIALOG_H
