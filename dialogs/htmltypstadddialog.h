#ifndef HTMLTYPSTADDDIALOG_H
#define HTMLTYPSTADDDIALOG_H

#include <QDialog>
#include <QProcess>

namespace Ui {
class HtmlTypstAddDialog;
}

class HtmlTypstAddDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HtmlTypstAddDialog(QWidget *parent = nullptr);
    ~HtmlTypstAddDialog();
    void reset();
    void setQId(int newQId);

    QString getRetPath() const;
    void setText(const QString &text);

private slots:
    void on_lineEdit_textChanged(const QString &text);
    void on_buttonBox_accepted();
    void onTypstWatcher_standard_output();

private:
    void stopWatcher();
    void startWatcherIfNeeded();
    QString contentHash() const;

    Ui::HtmlTypstAddDialog *ui;
    QProcess typstWatchProcess{this}; // typst 实时预览进程
    int questionId;

    QString retPath;
};

#endif // HTMLTYPSTADDDIALOG_H