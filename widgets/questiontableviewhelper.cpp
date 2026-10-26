#include "questiontableviewhelper.h"

#include <QHeaderView>

#include "utils/toolfunctions.h"

namespace {
constexpr int kQuestionStatusColumn = 2; // questions 自状态列起连续排列复盘字段
constexpr int kQuestionAverageRatingColumn = 3;
constexpr int kQuestionGoodTimeColumn = 4;
constexpr int kQuestionAverageTimeColumn = 5;
constexpr int kQuestionBestTimeColumn = 6;
constexpr int kQuestionDueColumn = 7;
constexpr int kQuestionLastStudyColumn = 8;

constexpr int kAnswerStatusColumn = 5; // clozes 的复盘字段整体比 questions 后移三列
constexpr int kAnswerAverageRatingColumn = 6;
constexpr int kAnswerGoodTimeColumn = 7;
constexpr int kAnswerAverageTimeColumn = 8;
constexpr int kAnswerBestTimeColumn = 9;
constexpr int kAnswerDueColumn = 10;
constexpr int kAnswerLastStudyColumn = 11;
}

void setQuestionTableHeaders(QSqlRelationalTableModel *model)
{
    model->setHeaderData(QuestionNameColumn, Qt::Horizontal, QStringLiteral("名称"));
    model->setHeaderData(kQuestionStatusColumn, Qt::Horizontal, QStringLiteral("状态"));
    model->setHeaderData(kQuestionAverageRatingColumn, Qt::Horizontal, QStringLiteral("平均评分"));
    model->setHeaderData(kQuestionGoodTimeColumn, Qt::Horizontal, QStringLiteral("良好用时"));
    model->setHeaderData(kQuestionAverageTimeColumn, Qt::Horizontal, QStringLiteral("平均用时"));
    model->setHeaderData(kQuestionBestTimeColumn, Qt::Horizontal, QStringLiteral("最佳用时"));
    model->setHeaderData(kQuestionDueColumn, Qt::Horizontal, QStringLiteral("到期"));
    model->setHeaderData(kQuestionLastStudyColumn, Qt::Horizontal, QStringLiteral("最近学习"));
}

void setupQuestionTableView(QTableView *view, QSqlRelationalTableModel *model)
{
    view->setModel(model); // 模型由调用方负责创建与释放
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setSortingEnabled(true);
    view->setColumnHidden(QuestionOrderNumColumn, true);
    view->setColumnHidden(QuestionAbstractNameColumn, true);

    ToolFunctions::fetchAllRows(model); // 取全量后一次性定宽，之后列宽完全交给用户拖动列边线
    view->horizontalHeader()->setStretchLastSection(false);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    view->resizeColumnsToContents();

    setQuestionTableHeaders(model);
}

void setAnswerTableHeaders(QSqlTableModel *model)
{
    model->setHeaderData(kAnswerStatusColumn, Qt::Horizontal, QStringLiteral("状态"));
    model->setHeaderData(kAnswerAverageRatingColumn, Qt::Horizontal, QStringLiteral("平均评分"));
    model->setHeaderData(kAnswerGoodTimeColumn, Qt::Horizontal, QStringLiteral("良好用时"));
    model->setHeaderData(kAnswerAverageTimeColumn, Qt::Horizontal, QStringLiteral("平均用时"));
    model->setHeaderData(kAnswerBestTimeColumn, Qt::Horizontal, QStringLiteral("最佳用时"));
    model->setHeaderData(kAnswerDueColumn, Qt::Horizontal, QStringLiteral("到期"));
    model->setHeaderData(kAnswerLastStudyColumn, Qt::Horizontal, QStringLiteral("最近学习"));
}