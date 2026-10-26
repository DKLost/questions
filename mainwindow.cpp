#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mainwindow_internal.h"

#include "parser/noteparser2.h"
#include "core/knowledgedb.h"
#include "widgets/knowledgetreehelper.h"
#include "widgets/questiontableviewhelper.h"
#include "dialogs/phenadddialog.h"
#include "utils/toolfunctions.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QSettings>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QList>
#include <QMenu>
#include <QContextMenuEvent>
#include <QMessageBox>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QSize>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardItem>
#include <QStringList>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextFrameFormat>
#include <QTextImageFormat>
#include <QTextList>
#include <QTextListFormat>
#include <QTextTableFormat>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kInitialSplitterLeft = 600;
constexpr int kInitialSplitterRight = 250;
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initFonts();
    initQuestionDatabase();
    init_questionTableView();
    init_answerTableView();
    init_answerTreeWidget();
    initQuestionEditor();
    initDialogs();
    initKnowledgeTree();
    initSplitter();
    questionTableModel->setFilter(""); // 知识关联区显示全部题目，不按分类过滤
    questionTableModel->select();
    setQuestionAreasEnabled(false); // 启动初始无选中题：html/cloze 编辑区先置灰空态（restoreUiState 选中题后自动恢复）
    restoreUiState(); // 恢复上次退出时的检索条件/展开集/当前题（无 ini 时为无操作）
}

// 退出前落盘：正在编辑的题面 + 界面状态
void MainWindow::closeEvent(QCloseEvent *event)
{
    saveCurrentQuestionContent();
    saveUiState();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveUiState()
{
    QSettings settings(kUiStateFileName, QSettings::IniFormat);
    settings.setValue(kUiStateFindActiveKey, findObjsActive);
    settings.setValue(kUiStateSubjKey, ui->subjLineEdit->text());
    settings.setValue(kUiStatePredKey, ui->predLineEdit->text());
    settings.setValue(kUiStateObjKey, ui->objLineEdit->text());
    settings.setValue(kUiStateNameKey, ui->lineEdit->text());
    settings.setValue(kUiStateScopeKey, scopeDerivedAbs.join(QLatin1Char('\n'))); // 用换行而非逗号：概念名里可能含逗号
    settings.setValue(kUiStateExpandedKey, phenIdsToCsv(collectExpandedPhenIds()));
    settings.setValue(kUiStateCurrentQuestionKey, currentQuestionId);
}

// 恢复顺序：检索框 → （检索态重检 / 否则恢复范围） → 展开集 → 当前题；命中不了的项静默跳过
void MainWindow::restoreUiState()
{
    QSettings settings(kUiStateFileName, QSettings::IniFormat);
    const QString subj = settings.value(kUiStateSubjKey).toString();
    const QString pred = settings.value(kUiStatePredKey).toString();
    const QString obj = settings.value(kUiStateObjKey).toString();
    {
        const QSignalBlocker subjBlocker(ui->subjLineEdit);
        const QSignalBlocker predBlocker(ui->predLineEdit);
        const QSignalBlocker objBlocker(ui->objLineEdit);
        const QSignalBlocker nameBlocker(ui->lineEdit);
        ui->subjLineEdit->setText(subj);
        ui->predLineEdit->setText(pred);
        ui->objLineEdit->setText(obj);
        ui->lineEdit->setText(settings.value(kUiStateNameKey).toString());
    }
    const bool findActive = settings.value(kUiStateFindActiveKey).toBool();
    if(findActive)
        applyFindObjs(); // 检索态：按恢复后的条件重跑检索（范围由其结果重新计算；条件全空即回到全树）
    else
        setQuestionScope(settings.value(kUiStateScopeKey).toString().split(QLatin1Char('\n'), Qt::SkipEmptyParts));
    restoreExpandedPhenIds(phenIdsFromCsv(settings.value(kUiStateExpandedKey).toString()));
    const int questionId = settings.value(kUiStateCurrentQuestionKey, -1).toInt();
    if(questionId > 0)
        set_and_jump_to_qId(questionId); // 命中不了的 id 静默跳过（on_idLineEdit_returnPressed 内部先查存在性）
}

// 全应用统一等宽字体：正文 12、小号 9
void MainWindow::initFonts()
{
    const QFont uiFont(kUiFontFamilies, kUiFontSize);
    QApplication::setFont(uiFont);
    ui->questionTextEdit->setFont(uiFont);
    ui->answerTreeWidget->setFont(uiFont);
    // 知识树显式设字体：.ui 里带（空）styleSheet 的控件会被 QStyleSheetStyle 在 setupUi 时按当时的默认字体“钉住”，
    // 之后再 QApplication::setFont 也推不进去（表现为树停在 9pt 而其余界面是 12pt），故与上面两处一样显式设定
    ui->phenTreeView->setFont(uiFont);
    ui->phenObjTreeView->setFont(uiFont);
    ui->groupBox_2->setFont(QFont(kUiFontFamilies, kSmallFontSize));
}

void MainWindow::initQuestionDatabase()
{
    currentDate = QDate::currentDate();
    questionSql = new QuestionSql(kDatabaseFileName, this);
    answerStore = new AnswerStore(questionSql);
}

void MainWindow::initQuestionEditor()
{
    ui->questionTextEdit->setTabStopDistance(ui->questionTextEdit->fontMetrics().horizontalAdvance(' ') * kEditorTabStopSpaces);
    ui->questionTextEdit->document()->setIndentWidth(kEditorIndentWidth);
    ui->questionTextEdit->viewport()->installEventFilter(this); // 双击 typst 公式图片 → 改公式
}

void MainWindow::initDialogs()
{
    learningDialog = new LearningDialog(questionSql, this);
    setTimeDialog = new SetTimeDialog(this);
    htmlTableAddDialog = new HtmlTableAddDialog(this);
    bindAnswerDialog = new BindAnswerDialog{questionSql, ui->phenTreeView, ui->questionTableView, this};
    answerEditDialog = new AnswerEditDialog(bindAnswerDialog, this);
    clozeMoveDialog = new ClozeMoveDialog(questionSql, this); // 答案迁移到其他题目
    bindAnswerDialog->setTreeModel(nullptr);
    bindAnswerDialog->disableCategoryTree(); // 分类树已禁用（知识索引区取代分类）
    descAddDialog = new DescAddDialog(this);
    htmlTypstAddDialog = new HtmlTypstAddDialog(this);
}

void MainWindow::initKnowledgeTree()
{
    reload_knowledgeTreeView();                                // 主树固定显示知识索引区（分类功能已移除）
    installMergeToggle(ui->phenTreeView);                      // ⇒ 合并行随展开状态切换文本
    installCollapseMergeOnLeave(ui->phenTreeView);             // 离开 is 子行自动折叠 ⇒ 父行
    installMiddleClickExpandAllExceptMerged(ui->phenTreeView); // 中键点击行展开整棵子树（不含 ⇒ 合并行）
    ui->phenTreeView->setContextMenuPolicy(Qt::CustomContextMenu); // 右键菜单：强制按概念子树过滤题目表
    ui->phenTreeView->installEventFilter(this);                    // 键盘改变选中行时触发与点击相同的载题行为
    connectKnowledgeTreeEntered();
    initPhenObjTree();
}

// 宾词视图与主树共用同一套渲染与交互（⇒ 合并、中键展开、右键菜单、点行载题）；入口只有 ctrl+左键与右键菜单，故默认隐藏
void MainWindow::initPhenObjTree()
{
    phenObjItemModel = new QStandardItemModel(this);
    ui->phenObjTreeView->setModel(phenObjItemModel);
    setupKnowledgeTreeView(ui->phenObjTreeView);
    installMergeToggle(ui->phenObjTreeView);
    installCollapseMergeOnLeave(ui->phenObjTreeView);
    installMiddleClickExpandAllExceptMerged(ui->phenObjTreeView);
    ui->phenObjTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->phenObjTreeView->installEventFilter(this);           // 聚焦跟踪 + 键盘换行载题（与主树一致）
    ui->phenObjTreeView->setVisible(false);                  // 未触发前不占空间
    connect(ui->phenObjTreeView, &QTreeView::clicked, this, &MainWindow::onPhenObjTreeClicked);
    connect(ui->phenObjTreeView, &QTreeView::customContextMenuRequested,
            this, &MainWindow::onPhenObjTreeCustomContextMenuRequested);
    ui->phenObjTreeView->viewport()->installEventFilter(this); // ctrl+左键在宾词视图内同样可再取宾词
    ui->phenTreeView->viewport()->installEventFilter(this);
}

void MainWindow::connectKnowledgeTreeEntered()
{
    // 按住左键拖动扫过树行时也触发载题（与题目表 entered 一致；未按按钮的悬停、按住右键、ctrl 手势均不触发）
    connect(ui->phenTreeView, &QTreeView::entered, this, [this](const QModelIndex &index) {
        if(QGuiApplication::mouseButtons() == Qt::NoButton || QGuiApplication::mouseButtons() == Qt::RightButton)
            return;
        if(QGuiApplication::keyboardModifiers() & Qt::ControlModifier)
            return; // ctrl+左键是"看宾词"手势，不跟随扫过的行载题
        ui->phenTreeView->setCurrentIndex(index); // 选中高亮跟随扫过的行
        on_phenTreeView_clicked(index);
    });
}

void MainWindow::initSplitter()
{
    ui->splitter->setSizes({kInitialSplitterLeft, kInitialSplitterRight});
}

MainWindow::~MainWindow()
{
    delete knowledgeRoot;
    delete phenObjKnowledgeRoot;
    delete ui;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if(watched == ui->questionTextEdit->viewport() && event->type() == QEvent::MouseButtonDblClick)
        return editTypstImageAt(static_cast<QMouseEvent *>(event)->position().toPoint());
    if(watched == ui->questionTableView)
        return handleQuestionTableKeyPress(event);
    if(event->type() == QEvent::ContextMenu) // ctrl+右键＝弹出题目预览（吞掉菜单，见 previewQuestionAt）
    {
        QTreeView *view = nullptr;
        if(watched == ui->phenTreeView || watched == ui->phenTreeView->viewport())
            view = ui->phenTreeView;
        else if(watched == ui->phenObjTreeView || watched == ui->phenObjTreeView->viewport())
            view = ui->phenObjTreeView;
        const auto *menuEvent = static_cast<QContextMenuEvent *>(event);
        if(view && (menuEvent->modifiers() & Qt::ControlModifier)
           && previewQuestionAt(view, menuEvent->globalPos()))
            return true;
    }
    if(watched == ui->phenTreeView || watched == ui->phenObjTreeView)
    {
        if(event->type() == QEvent::FocusIn)
            lastFocusedPhenView = static_cast<QTreeView *>(watched); // 共用的增删改按钮以最后聚焦的知识视图为准
        return handlePhenViewKeyPress(static_cast<QTreeView *>(watched), event);
    }
    if(watched == ui->phenTreeView->viewport() || watched == ui->phenObjTreeView->viewport())
        return handlePhenViewCtrlClick(watched == ui->phenTreeView->viewport() ? ui->phenTreeView : ui->phenObjTreeView, event);
    return QMainWindow::eventFilter(watched, event); // 其余事件交回父类处理
}

bool MainWindow::handleQuestionTableKeyPress(QEvent *event)
{
    if(event->type() != QEvent::KeyPress)
        return false;
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if(keyEvent->key() == Qt::Key_Up)
        moveQuestionSelectionBy(-1);
    else if(keyEvent->key() == Qt::Key_Down)
        moveQuestionSelectionBy(1);
    return true;
}

// 严格边界：末行再按 Down / 首行再按 Up 不动作，避免 selectRow 越界产生无效行
void MainWindow::moveQuestionSelectionBy(int delta)
{
    const QModelIndex index = ui->questionTableView->currentIndex();
    if(!index.isValid())
        return;
    const int targetRow = index.row() + delta;
    if(targetRow < 0 || targetRow >= ui->questionTableView->model()->rowCount())
        return;
    ui->questionTableView->selectRow(targetRow);
    const QModelIndex next = ui->questionTableView->currentIndex();
    if(next.isValid())
        on_questionTableView_activated(next);
}

// 两视图共用：让 QTreeView 先原生处理按键，处理完后当前行若变化则触发与点击该行相同的载题行为
bool MainWindow::handlePhenViewKeyPress(QTreeView *view, QEvent *event)
{
    if(event->type() == QEvent::KeyPress)
    {
        const QModelIndex before = view->currentIndex();
        QTimer::singleShot(0, this, [this, view, before]() {
            const QModelIndex after = view->currentIndex();
            if(after.isValid() && after != before)
                phenRowActivated(view, after);
        });
    }
    return false;
}
