#include "services/appsettings.h"
#include "core/fsrs.h"
#include "services/typstimagecache.h"
#include "utils/toolfunctions.h"

#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <cmath>

namespace {
const QString kSettingsFileName = QStringLiteral("./settings.ini");
}

AppSettings AppSettings::load()
{
    QSettings store(kSettingsFileName, QSettings::IniFormat);
    AppSettings settings;

    bool weightsOk = false;
    const QList<double> savedWeights =
        parseWeights(store.value(QStringLiteral("fsrs/weights")).toString(), &weightsOk);
    if(weightsOk)
        settings.weights = savedWeights;
    else
        for(int i = 0; i < 21; ++i)
            settings.weights.append(FSRS::defaultW[i]);

    settings.requestRetention =
        qBound(0.70, store.value(QStringLiteral("fsrs/requestRetention"), 0.8).toDouble(), 0.97);
    settings.maximumInterval =
        qBound(1, store.value(QStringLiteral("fsrs/maximumInterval"), 36500).toInt(), 36500);
    settings.easyFactor =
        qBound(0.10, store.value(QStringLiteral("fsrs/easyFactor"), 0.5).toDouble(), 1.00);
    settings.hardFactor =
        qBound(0.50, store.value(QStringLiteral("fsrs/hardFactor"), 1.0).toDouble(), 3.00);
    settings.newCardTimeFactor =
        qBound(1.0, store.value(QStringLiteral("fsrs/newCardTimeFactor"), 2.0).toDouble(), 5.0);

    settings.typstCommand =
        store.value(QStringLiteral("env/typstCommand"), QStringLiteral("typst")).toString().trimmed();
    if(settings.typstCommand.isEmpty())
        settings.typstCommand = QStringLiteral("typst");

    settings.backupBeforeImport = store.value(QStringLiteral("data/backupBeforeImport"), true).toBool();
    settings.backupKeepCount =
        qBound(1, store.value(QStringLiteral("data/backupKeepCount"), 5).toInt(), 50);
    return settings;
}

void AppSettings::save() const
{
    QSettings store(kSettingsFileName, QSettings::IniFormat);
    store.setValue(QStringLiteral("fsrs/weights"), weightsToText(weights));
    store.setValue(QStringLiteral("fsrs/requestRetention"), requestRetention);
    store.setValue(QStringLiteral("fsrs/maximumInterval"), maximumInterval);
    store.setValue(QStringLiteral("fsrs/easyFactor"), easyFactor);
    store.setValue(QStringLiteral("fsrs/hardFactor"), hardFactor);
    store.setValue(QStringLiteral("fsrs/newCardTimeFactor"), newCardTimeFactor);
    store.setValue(QStringLiteral("env/typstCommand"), typstCommand);
    store.setValue(QStringLiteral("data/backupBeforeImport"), backupBeforeImport);
    store.setValue(QStringLiteral("data/backupKeepCount"), backupKeepCount);
    store.sync();
}

void AppSettings::apply() const
{
    FSRS::setWeights(weights);
    FSRS::requestRetention = requestRetention;
    FSRS::maximumInterval = maximumInterval;
    FSRS::easyFactor = easyFactor;
    FSRS::hardFactor = hardFactor;
    FSRS::newCardTimeFactor = newCardTimeFactor;

    ToolFunctions::setTypstCommand(typstCommand);
    TypstImageCache::instance()->setCommand(typstCommand);
}

QList<double> AppSettings::parseWeights(const QString &text, bool *ok)
{
    // 允许直接粘贴 optimizer 的列表输出：忽略方括号、中英文逗号、分号与空白
    const QStringList parts =
        text.split(QRegularExpression(QStringLiteral("[\\s,;，\\[\\]]+")), Qt::SkipEmptyParts);
    if(parts.size() != 21)
    {
        if(ok)
            *ok = false;
        return {};
    }

    QList<double> weights;
    weights.reserve(21);
    for(const QString &part : parts)
    {
        bool converted = false;
        const double value = part.toDouble(&converted);
        if(!converted || !std::isfinite(value) || value < 0.0)
        {
            if(ok)
                *ok = false;
            return {};
        }
        weights.append(value);
    }
    if(!(weights[20] > 0.0)) // w[20] 是衰减参数，必须为正才能算 forgetting curve
    {
        if(ok)
            *ok = false;
        return {};
    }
    if(ok)
        *ok = true;
    return weights;
}

QString AppSettings::weightsToText(const QList<double> &weights)
{
    QStringList parts;
    parts.reserve(weights.size());
    for(double value : weights)
        parts.append(QString::number(value, 'g', 12));
    return parts.join(QStringLiteral(", "));
}
