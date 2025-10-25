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
#include "core/questionsql.h"
#include <QLabel>
#include <QSqlTableModel>
#include <QSqlRelationalTableModel>
#include <QRandomGenerator>
#include <QSqlRecord>
#include <QJsonObject>
#include <QCheckBox>
#include <QLayout>
#include <QProcess>
#include <limits>

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


    void set_category(int categoryId, bool learnALl);
    void set_question(int id);

    void start_timer();
    void stop_timer();

    void clear_question_display();

    QTime getTime() const;
    bool is_submited();

    void set_tableHeader();
    void set_items_table(QString filter);

    QTime getTotalTime() const;

    void setIsSpeedLearn(bool newIsSpeedLearn);


    int getLastId() const;

    bool getOnlyToLearn() const;
    void setOnlyToLearn(bool newOnlyToLearn);
    void highlight_blank_by_number(int number);
private slots:
    void on_pushButton_clicked();
    void timerHandler();
    void totalTimerHandler();

    void on_LearningDialog_finished(int result);

    void on_tableView_clicked(const QModelIndex &index);

    void on_comboBox_currentTextChanged(const QString &arg1);

    void answer_lineEdit_textChanged(const QString &arg1);
    void poolComboBox_currentIndexChanged(const int &index);
    void onTypstWatcher_standard_output();

private:
    Ui::LearningDialog *ui;

    QTimer *timer;
    QTime time;

    QTimer *totalTimer;
    QTime totalTime;
    int totalCount;
    int correctCount;
    bool isSpeedLearn;

    bool preSubmited;
    bool submited;
    QString rating;
    QFont font;
    QFontMetrics *fm;
    QuestionSql *questionSql;
    QLabel* timeLabel;
    QSqlRelationalTableModel *tableModel;
    int currentId;
    QJsonArray currentArray;
    int oldRow;

    int lastId;

    bool onlyToLearn;

    int rowCount;
    void preSubmit();
    void submit();

    QLineEdit* currentLineEdit;
    QProcess typstWatchProcess{this};

    QTime totalGoodTime;
    int totalRemainingTimeMsec;

protected:
    bool eventFilter(QObject *watched, QEvent *event);

};

#endif // LEARNINGDIALOG_H
