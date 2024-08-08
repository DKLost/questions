#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QSqlTableModel>
#include <QSqlRelationalTableModel>
#include <QKeyEvent>
#include <QList>
#include <QListWidget>
#include <QProcess>
#include <QFileDialog>
#include "learningdialog.h"
#include "questionsql.h"
#include "questionmovedialog.h"
#include "answereditdialog.h"
#include "settimedialog.h"
#include "htmltableadddialog.h"
#include "questiontageditdialog.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void get_categoryItemTree(QStandardItem *parent, int parentId);
    // void display_category_name();
    void reload_categoryTreeView();

    void save_answerList(int id);
    void set_tableHeader();
protected:
    bool eventFilter(QObject *watched, QEvent *event);

private slots:
    void on_categoryAddButton_clicked();
    void on_categoryDelButton_clicked();
    void on_categoryTreeView_clicked(const QModelIndex &index);
    void on_questionAddButton_clicked();
    void on_questionTextEdit_textChanged();
    void on_questionTableView_clicked(const QModelIndex &index);
    void on_questionDelButton_clicked();
    void on_questionMoveButton_clicked();
    void on_questionTableView_activated(const QModelIndex &index);

    void on_answerAddButton_clicked();

    void on_answerDelButton_clicked();

    void on_answerEditButton_clicked();

    void on_itemLearnButton_clicked();

    void on_setGoodTimeButton_clicked();

    void on_categoryLearnButton_clicked();

    void on_categoryToLearnButton_clicked();

    void on_answerListWidget_itemDoubleClicked(QListWidgetItem *item);

    void on_htmlTableAddButton_clicked();

    void on_categoryEditButton_clicked();

    void category_item_change_handler(QStandardItem *item);

    void on_addTagButton_clicked();

    void on_delTagButton_clicked();

    void on_questionTagButton_clicked();

    void on_tagTableView_activated(const QModelIndex &index);

    void on_tagTableView_clicked(const QModelIndex &index);

    void on_speedLearnButton_clicked();

    void on_questionDirOpenButton_clicked();

    void on_answerListRowsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationRow);

    void on_htmlImgAddButton_clicked();

private:
    Ui::MainWindow *ui;

    QuestionSql *questionSql;
    QStandardItemModel *categoryItemModel;
    QMap<int,QList<QStandardItem*>> categoryItemLists;
    QSqlRelationalTableModel *questionTableModel;
    bool is_questionTextEdit_editable = false;

    QSqlTableModel *tagTableModel;
    int currentSection;

    QuestionMoveDialog *questionMoveDialog;
    AnswerEditDialog *answerEditDialog;
    LearningDialog *learningDialog;
    SetTimeDialog *setTimeDialog;
    HtmlTableAddDialog *htmlTableAddDialog;
    QuestionTagEditDialog *questionTagEditDialog;

};
#endif // MAINWINDOW_H
