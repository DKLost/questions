#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QList>
#include <QString>

// 用户可调设置：学习算法、外部工具、导入前备份；持久化到运行目录的 settings.ini
struct AppSettings
{
    QList<double> weights;          // FSRS 21 个权重（w[0]…w[20]）
    double requestRetention = 0.8;  // 目标保留率
    int maximumInterval = 36500;    // 最大间隔（天）
    double easyFactor = 0.5;        // 用时 < 良好用时 × easyFactor → easy
    double hardFactor = 1.0;        // 用时 > 良好用时 × hardFactor → hard
    double newCardTimeFactor = 2.0; // 新卡首次作答用时 × 该系数 = 初始良好用时
    QString typstCommand = QStringLiteral("typst");
    bool backupBeforeImport = true; // parser 导入会清空知识库，默认先备份
    int backupKeepCount = 5;        // 备份保留份数

    static AppSettings load(); // 无 settings.ini 或值非法时回落到默认
    void save() const;
    void apply() const; // 写入 FSRS 静态参数 / ToolFunctions / TypstImageCache

    // 21 个数字，用逗号、空格、分号或换行分隔；非法时返回空列表并置 ok=false
    static QList<double> parseWeights(const QString &text, bool *ok = nullptr);
    static QString weightsToText(const QList<double> &weights);
};

#endif // APPSETTINGS_H
