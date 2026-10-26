#ifndef MAINWINDOW_INTERNAL_H
#define MAINWINDOW_INTERNAL_H

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextImageFormat>
#include <QTreeWidgetItem>

inline const QStringList kUiFontFamilies = {"IBM Plex Mono", "IBM Plex Math", "Noto Sans Mono",
                                     "Microsoft YaHei UI", "Microsoft YaHei", "Arial"};
inline constexpr int kUiFontSize = 12;
inline constexpr int kSmallFontSize = 9;
inline constexpr int kEditorTabStopSpaces = 2;
inline constexpr double kEditorIndentWidth = 32.5; // 题面固定缩进
// 库表名与字段（与 core 的 questions/clozes 表结构一致）
inline const QString kDatabaseFileName = QStringLiteral("question.db");
inline const QString kQuestionsTable = QStringLiteral("questions");
inline const QString kClozesTable = QStringLiteral("clozes");
inline const QString kAbstractNameField = QStringLiteral("abstractName");
inline const QString kNextDateField = QStringLiteral("nextDate");
inline const QString kGoodTimeField = QStringLiteral("goodTime");
inline const QString kInjectField = QStringLiteral("inject");
inline const QString kConnectionName = QStringLiteral("connection1");
inline const QString kGoodTimeFormat = QStringLiteral("mm'm':ss's'"); // goodTime 在库中的存储格式
inline const QString kDateStorageFormat = QStringLiteral("yyyy/MM/dd"); // nextDate 在库中的存储格式
// 界面状态持久化（cwd 下的 ini，与 db/data 的文件式布局一致；清空该文件即恢复出厂默认）
inline const QString kUiStateFileName = QStringLiteral("./ui_state.ini");
inline const QString kUiStateFindActiveKey = QStringLiteral("findObjs/active");
inline const QString kUiStateSubjKey = QStringLiteral("findObjs/subject");
inline const QString kUiStatePredKey = QStringLiteral("findObjs/predicate");
inline const QString kUiStateObjKey = QStringLiteral("findObjs/object");
inline const QString kUiStateNameKey = QStringLiteral("question/nameFilter");
inline const QString kUiStateScopeKey = QStringLiteral("question/scopeDerivedAbs");
inline const QString kUiStateExpandedKey = QStringLiteral("tree/expandedPhenIds");
inline const QString kUiStateCurrentQuestionKey = QStringLiteral("question/currentId");

inline QString phenIdsToCsv(const QSet<int> &ids)
{
    QStringList parts;
    for(int id : ids)
        parts.append(QString::number(id));
    return parts.join(QLatin1Char(','));
}
inline QSet<int> phenIdsFromCsv(const QString &csv)
{
    QSet<int> ids;
    for(const QString &part : csv.split(QLatin1Char(','), Qt::SkipEmptyParts))
        ids.insert(part.toInt());
    return ids;
}
inline int rowOfAnswerItem(QTreeWidgetItem *item)
{
    return item->text(0).toInt() - 1;
}

inline QString readTextFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

// typst 公式图片 → 同目录 <hash>.typmath 源码路径（图片名形如 data/<题>/<hash>.png；无源码文件返回空）
inline QString typstSourcePathOf(const QTextImageFormat &imageFormat)
{
    const QFileInfo info(imageFormat.name());
    if(info.suffix().compare(QStringLiteral("png"), Qt::CaseInsensitive) != 0)
        return {};
    const QString sourcePath = info.path() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".typmath");
    return QFile::exists(sourcePath) ? sourcePath : QString();
}

#endif // MAINWINDOW_INTERNAL_H
