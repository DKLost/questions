#include "mainwindow.h"
#include "mainwindow_internal.h"
#include "services/statistics.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QDate>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

namespace {
// 工具栏用的紧凑时长：0m / <1m / 21m / 1h05m
QString compactDuration(qint64 ms)
{
    if(ms <= 0)
        return QStringLiteral("0m");
    const qint64 minutes = ms / 60000;
    if(minutes == 0)
        return QStringLiteral("<1m");
    if(minutes < 60)
        return QStringLiteral("%1m").arg(minutes);
    return QStringLiteral("%1h%2m").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}
} // namespace

// 统计工具栏：待学 / 今日复习 / 今日新学 / 今日用时；只读，避免打断学习节奏
void MainWindow::initStatisticsBar()
{
    statisticsBar = addToolBar(QStringLiteral("统计"));
    statisticsBar->setObjectName(QStringLiteral("statisticsToolBar"));
    statisticsBar->setMovable(false);
    statisticsBar->setFloatable(false);

    QFont statsFont = QApplication::font();
    statsFont.setPointSize(10);

    auto makeLabel = [this, &statsFont]() {
        auto *label = new QLabel(statisticsBar);
        label->setFont(statsFont);
        label->setTextFormat(Qt::PlainText);
        label->setContentsMargins(10, 0, 10, 0);
        return label;
    };
    auto addSeparator = [this]() {
        auto *line = new QFrame(statisticsBar);
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);
        statisticsBar->addWidget(line);
    };

    // “工具”菜单直接挂在工具栏上，与统计同一行（主菜单栏已隐藏）
    auto *toolMenu = new QMenu(statisticsBar);
    QAction *settingsAct = toolMenu->addAction(QStringLiteral("设置…"));
    connect(settingsAct, &QAction::triggered, this, &MainWindow::openSettings);
    toolMenu->addSeparator();
    QAction *exportRevlogAct = toolMenu->addAction(QStringLiteral("导出复习日志（FSRS optimizer）"));
    connect(exportRevlogAct, &QAction::triggered, this, &MainWindow::exportReviewLog);

    auto *toolButton = new QToolButton(statisticsBar);
    toolButton->setText(QStringLiteral("工具"));
    toolButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolButton->setPopupMode(QToolButton::InstantPopup);
    toolButton->setAutoRaise(true);
    toolButton->setMenu(toolMenu);
    statisticsBar->addWidget(toolButton);
    addSeparator();

    statsDueLabel = makeLabel();
    statsReviewLabel = makeLabel();
    statsLearnLabel = makeLabel();
    statsTimeLabel = makeLabel();

    statisticsBar->addWidget(statsDueLabel);
    addSeparator();
    statisticsBar->addWidget(statsReviewLabel);
    addSeparator();
    statisticsBar->addWidget(statsLearnLabel);
    addSeparator();
    statisticsBar->addWidget(statsTimeLabel);

    // 题目模型变化（过滤、重新 select、改到期日）后合并刷新，避免批量操作时反复查库
    statisticsRefreshTimer = new QTimer(this);
    statisticsRefreshTimer->setSingleShot(true);
    statisticsRefreshTimer->setInterval(200);
    connect(statisticsRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshStatistics);
    connect(questionTableModel, &QAbstractItemModel::modelReset, this,
            &MainWindow::scheduleStatisticsRefresh);
    connect(questionTableModel, &QAbstractItemModel::dataChanged, this,
            &MainWindow::scheduleStatisticsRefresh);

    // 应用跨天时（午夜）刷新“今日”口径
    statisticsDay = QDate::currentDate();
    statisticsDateTimer = new QTimer(this);
    statisticsDateTimer->setInterval(60000);
    connect(statisticsDateTimer, &QTimer::timeout, this, [this] {
        if(statisticsDay != QDate::currentDate())
            refreshStatistics();
    });
    statisticsDateTimer->start();
}

void MainWindow::scheduleStatisticsRefresh()
{
    if(statisticsRefreshTimer)
        statisticsRefreshTimer->start();
}

void MainWindow::refreshStatistics()
{
    if(!statsDueLabel || !questionSql)
        return;

    statisticsDay = QDate::currentDate();
    const QString filter = questionTableModel->filter();
    const StatisticsSnapshot stats = collectStatistics(questionSql->getDb(), filter);

    statsDueLabel->setText(QStringLiteral("待学 %1").arg(stats.dueQuestions));
    QString dueTip = QStringLiteral("逾期 %1 · 今日 %2")
                         .arg(stats.overdueQuestions)
                         .arg(stats.dueTodayQuestions);
    if(!filter.isEmpty())
        dueTip += QStringLiteral("\n当前范围 %1 题 · 全部 %2 题")
                      .arg(stats.dueQuestions)
                      .arg(stats.allDueQuestions);
    statsDueLabel->setToolTip(dueTip);

    statsReviewLabel->setText(QStringLiteral("今日复习 %1").arg(stats.reviewedCards));
    statsReviewLabel->setToolTip(QStringLiteral("%1 空 · %2 次作答\n用时 %3")
                                     .arg(stats.reviewedCards)
                                     .arg(stats.reviewAttempts)
                                     .arg(compactDuration(stats.reviewMs)));

    statsLearnLabel->setText(QStringLiteral("今日新学 %1").arg(stats.learnedCards));
    statsLearnLabel->setToolTip(QStringLiteral("%1 空（含复习失败后重学）\n用时 %2")
                                    .arg(stats.learnedCards)
                                    .arg(compactDuration(stats.learnMs)));

    statsTimeLabel->setText(QStringLiteral("今日用时 %1").arg(compactDuration(stats.totalMs)));
    statsTimeLabel->setToolTip(QStringLiteral("复习 %1 · 新学 %2\n统计自 review_log 启用后")
                                   .arg(compactDuration(stats.reviewMs), compactDuration(stats.learnMs)));
}
