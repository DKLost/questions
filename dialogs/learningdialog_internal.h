#ifndef LEARNINGDIALOG_INTERNAL_H
#define LEARNINGDIALOG_INTERNAL_H

#include <QtGlobal>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QList>
#include <QPair>
#include <QSize>

namespace learningdialog_internal {

// 计时与时间格式
inline const QString kGoodTimeFormat = QStringLiteral("mm'm':ss's'"); // goodTime 在库中的存储格式

// 库表名与字段（与 core 的 questions/clozes 表结构一致）
inline const QString kQuestionsTable = QStringLiteral("questions");
inline const QString kOrderNumField = QStringLiteral("orderNum");
inline const QString kAbstractNameField = QStringLiteral("abstractName");
inline const QString kGoodTimeField = QStringLiteral("goodTime");

// 评级文案
inline const QString kRatingGoodText = QStringLiteral("良好");

inline const QString kRandomSortMode = QStringLiteral("随机排序");
inline const QString kDefaultSortMode = QStringLiteral("默认排序");
inline constexpr int kRandomOrderMax = 32768;            // 随机排序号范围 0..32767
inline constexpr int kMaxOrderNum = 32767;

// 答案行控件尺寸
inline constexpr int kInputWidthPadding = 15; // 输入框最小宽度 = 文本宽度 + 补正
inline const QSize kIconLookupSize{1000, 1000};

// 网格列（答案不再单独占列：它随输入框"上下齐平"对照，见 learningdialog_grid.cpp 的 compareBox）
// 某行是否参与作答由结构判定（输入列是"输入行+对照行"的纵向容器 ⇒ 参与），见 rowParticipates
enum AnswerGridColumn { ColumnLineNumber = 0, ColumnInput = 1, ColumnCheck = 2, ColumnTime = 3, ColumnGoodTime = 4, ColumnRating = 5 };

// 门控演练（docs/02）：哨兵值、树行显示状态与提示文案
inline constexpr int kNoGateRow = -1;
inline constexpr int kNoGroup = -1;
inline constexpr int kNoQuestion = -1;
inline constexpr int kDrillStateNone = 0;
inline constexpr int kDrillStateCorrect = 2;
inline constexpr int kDrillStateWrong = 3;
inline const QString kMaskedBlankText = QStringLiteral("?");
inline const QString kTreeMergeArrow = QStringLiteral(" ⇒ ");

// 遮罩状态角色（挂在知识树 item 上；模型重建后随 item 销毁，无需手动清理）
inline constexpr int MaskedRole = Qt::UserRole + 20;          // 当前是否处于任一遮罩态（自身或子行后缀）
inline constexpr int HasOrigRole = Qt::UserRole + 21;         // 是否已备份原始文本
inline constexpr int OrigDisplayRole = Qt::UserRole + 22;
inline constexpr int OrigBaseRole = Qt::UserRole + 23;
inline constexpr int OrigFullRole = Qt::UserRole + 24;
inline constexpr int ChainDrillStateRole = Qt::UserRole + 25; // 门控演练：0=无 / 1=已回填 / 2=正确揭示 / 3=错误揭示

}

#endif // LEARNINGDIALOG_INTERNAL_H