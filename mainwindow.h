#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QTreeWidgetItem>
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
#include "core/questionsql.h"
#include "questionmovedialog.h"
#include "answereditdialog.h"
#include "settimedialog.h"
#include "htmltableadddialog.h"
#include "toolfunctions.h"
#include "questionrenamedialog.h"
#include "bindanswerdialog.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
    friend class BindAnswerDialog;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    //answer
    void load_answer(int qId);
    void init_answerTableView();
    void set_answer_tableHeader();
    void init_answerTreeWidget();

    //question
    void init_questionTableView();
    int select_question_by_id(int id);
    void set_quesion_tableHeader();

    //category
    void get_categoryItemTree(QStandardItem *parent, int parentId);
    void reload_categoryTreeView();

    void update_count_categoryTreeView();


protected:
    bool eventFilter(QObject *watched, QEvent *event);


private slots:
    //answer
    void on_answerAddButton_clicked();
    void on_answerDelButton_clicked();
    void on_answerEditButton_clicked();
    void on_answerTreeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_answerTreeWidget_itemChanged(QTreeWidgetItem *item, int column);
    void on_answerTreeWidget_itemActivated(QTreeWidgetItem *item, int column);
    void on_answerTreeWidget_itemClicked(QTreeWidgetItem *item, int column);
    void answerItemRowsMoved(int startIndex, int endIndex);

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
    void on_questionResetButton_clicked();

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

    //other
    void on_setFontButton_clicked();
    void on_setGoodTimeButton_clicked();

    void on_questionRenameButton_clicked();





    void on_answerTreeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);



private:
    Ui::MainWindow *ui;

    //model
    QSqlRelationalTableModel *questionTableModel;
    QSqlTableModel *answerTableModel;
    QStandardItemModel *categoryItemModel;

    //dialog
    QuestionMoveDialog *questionMoveDialog;
    AnswerEditDialog *answerEditDialog;
    LearningDialog *learningDialog;
    SetTimeDialog *setTimeDialog;
    HtmlTableAddDialog *htmlTableAddDialog;
    QuestionRenameDialog questionRenameDialog;
    BindAnswerDialog *bindAnswerDialog;

    //other
    QuestionSql *questionSql;
    QMap<int,QList<QStandardItem*>> categoryItemLists;
    bool is_questionTextEdit_editable = false;
    int currentSection;
    QDate currentDate;

};
#endif // MAINWINDOW_H
