#ifndef LEARNINGDIALOG_H
#define LEARNINGDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QJsonArray>
#include <QLayoutItem>
#include <QLineEdit>
#include <QFont>
#include <QFontMetrics>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "questionsql.h"
#include <QLabel>
#include <QSqlTableModel>
#include <QSqlRelationalTableModel>
#include <QRandomGenerator>
#include <QSqlRecord>

namespace Ui {
class LearningDialog;
}

class LearningDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LearningDialog(QuestionSql *newQuestionSql,QWidget*parent = nullptr);
    ~LearningDialog();

    void set_answer(QJsonArray array);
    void start_timer();

    void set_category(int categoryId, bool learnALl);
    void set_question(int id);

    void stop_timer();
    void clear_question_display();

    QTime getTime() const;
    bool is_submited();

    void set_tableHeader();
    void set_itmes_table(QString filter);

private slots:
    void on_pushButton_clicked();
    void timerHandler();

    void on_LearningDialog_finished(int result);

    void on_tableView_clicked(const QModelIndex &index);

    void on_comboBox_currentTextChanged(const QString &arg1);

private:
    Ui::LearningDialog *ui;
    QTimer *timer;
    QTime time;
    bool submited;
    QString rating;
    QFont *font;
    QFontMetrics *fm;
    QuestionSql *questionSql;
    QLabel* timeLabel;
    QSqlRelationalTableModel *tableModel;
    int currentId;
    int oldRow;
};

#endif // LEARNINGDIALOG_H