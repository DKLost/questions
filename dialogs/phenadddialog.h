#ifndef PHENADDDIALOG_H
#define PHENADDDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class PhenAddDialog;
}

// 新增现象弹窗：f/x ⇒ r/m 四段 + (∃) 形式(:/@)，对应笔记语法 - has:f : x ⇒ r : m（f 缺省 has，r 缺省 is）
class PhenAddDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PhenAddDialog(QWidget *parent = nullptr);
    ~PhenAddDialog();

    // 结果读取（accept 后有效；f/r 已按 has/is 缺省回填）
    QString getF() const;
    QString getX() const;
    QString getR() const;
    QString getM() const;
    bool getAtFlag1() const; // true = '@'，false = ':'
    bool getAtFlag2() const;
    bool isTopLevel() const; // 顶层概念勾选状态
    void setTopLevel(bool checked, bool disable = false); // 外部预设；disable=true 时勾选且不可取消

    // 编辑既有现象行的预填（f/x/r/m 为显示名，与 get* 同口径）；挂载点随行现状固定不可改（改挂载点走移动按钮）
    void setup(const QString &f, const QString &x, bool atFlag1,
               const QString &r, const QString &m, bool atFlag2, bool topLevel);

    // 覆写：xLineEdit 为空则拒绝；mLineEdit 为空则 ⇒ 后部分作废
    void accept() override;

private slots:
    void on_fLineEdit_3_textChanged(const QString &);
    void on_xLineEdit_3_textChanged(const QString &);
    void on_rLineEdit_3_textChanged(const QString &);
    void on_mLineEdit_3_textChanged(const QString &);
    void on_topLevelCheckBox_toggled(bool checked);

private:
    void fitWidth(class QLineEdit *edit);
    void syncFPredicateDefault(); // 同步 f 缺省谓词与占位符（: has / @ ~has）
    QString readFPredicate() const; // f 为空则按 (∃) 形式回填缺省谓词
    void readRelationAndObject(); // m 为空则 r/m 一并作废

    Ui::PhenAddDialog *ui;
    QString retF, retX, retR, retM;
    bool isAtPrefixedF = false, isAtPrefixedR = false;
};

#endif // PHENADDDIALOG_H