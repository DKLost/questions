#ifndef QUESTIONTABLEVIEWHELPER_H
#define QUESTIONTABLEVIEWHELPER_H

#include <QSqlRelationalTableModel>
#include <QSqlTableModel>
#include <QTableView>

// 题目表（questions）与答案表（clozes）在各视图中的共用初始化：列标题、只读整行选择、排序与列宽策略集中于此，避免各对话框重复且口径漂移。

enum QuestionColumn // questionTableModel 的列下标（与 QuestionSql::headerColumnMap 一致）
{
    QuestionIdColumn = 0,
    QuestionNameColumn = 1,
    QuestionOrderNumColumn = 9,
    QuestionAbstractNameColumn = 10
};

void setQuestionTableHeaders(QSqlRelationalTableModel *model);

// 只读、整行多选、可排序；隐藏顺序号与概念列；全列 Interactive 并按内容定宽一次，末列不随窗口拉伸
void setupQuestionTableView(QTableView *view, QSqlRelationalTableModel *model);

void setAnswerTableHeaders(QSqlTableModel *model);

#endif // QUESTIONTABLEVIEWHELPER_H