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
#include <QFontDialog>
#include <QJsonObject>
#include "learningdialog.h"
#include "questionsql.h"
#include "questionmovedialog.h"
#include "answereditdialog.h"
#include "settimedialog.h"
#include "htmltableadddialog.h"
#include "questiontageditdialog.h"
#include "toolfunctions.h"
#include "questionrenamedialog.h"

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

    //answer
    void init_answerTableView();
    void set_answer_tableHeader();

    //question
    void init_questionTableView();
    void select_question_by_id(int id);
    void set_quesion_tableHeader();

    //category
    void get_categoryItemTree(QStandardItem *parent, int parentId);
    void reload_categoryTreeView();


    void save_answerList(int id);

    void update_count_categoryTreeView();



protected:
    bool eventFilter(QObject *watched, QEvent *event);

private slots:


    void on_answerAddButton_clicked();
    void on_answerDelButton_clicked();
    void on_answerEditButton_clicked();
    void on_answerListWidget_itemDoubleClicked(QListWidgetItem *item);
    void on_answerListRowsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationRow);
    void on_answerListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

    //question
    void on_questionDirOpenButton_clicked();
    void on_questionAddButton_clicked();
    void on_questionTextEdit_textChanged();
    void on_questionTableView_clicked(const QModelIndex &index);
    void on_questionDelButton_clicked();
    void on_questionMoveButton_clicked();
    void on_questionTableView_activated(const QModelIndex &index);
    void on_questionTableView_pressed(const QModelIndex &index);
    void on_questionTableView_entered(const QModelIndex &index);

    //learn
    void on_itemLearnButton_clicked();
    void on_categoryLearnButton_clicked();
    void on_categoryToLearnButton_clicked();
    void on_speedLearnButton_clicked();

    //html edit
    void on_htmlImgAddButton_clicked();
    void on_htmlTableAddButton_clicked();

    //category
    void on_categoryAddButton_clicked();
    void on_categoryDelButton_clicked();
    void on_categoryTreeView_clicked(const QModelIndex &index);
    void category_item_change_handler(QStandardItem *item);
    void on_categoryEditButton_clicked();

    //tag
    void on_addTagButton_clicked();
    void on_delTagButton_clicked();
    void on_questionTagButton_clicked();
    void on_tagTableView_activated(const QModelIndex &index);
    void on_tagTableView_clicked(const QModelIndex &index);

    //other
    void on_setFontButton_clicked();
    void on_setGoodTimeButton_clicked();

    void on_questionRenameButton_clicked();

private:
    Ui::MainWindow *ui;

    //model
    QSqlRelationalTableModel *questionTableModel;
    QSqlTableModel *answerTableModel;
    QSqlTableModel *tagTableModel;
    QStandardItemModel *categoryItemModel;

    //dialog
    QuestionMoveDialog *questionMoveDialog;
    AnswerEditDialog *answerEditDialog;
    LearningDialog *learningDialog;
    SetTimeDialog *setTimeDialog;
    HtmlTableAddDialog *htmlTableAddDialog;
    QuestionTagEditDialog *questionTagEditDialog;
    QuestionRenameDialog questionRenameDialog;

    //other
    QuestionSql *questionSql;
    QMap<int,QList<QStandardItem*>> categoryItemLists;
    bool is_questionTextEdit_editable = false;
    int currentSection;
    QDate currentDate;
};
#endif // MAINWINDOW_H
