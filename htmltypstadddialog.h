#ifndef HTMLTYPSTADDDIALOG_H
#define HTMLTYPSTADDDIALOG_H

#include <QDialog>
#include <QProcess>
#include <QUuid>
#include <QCryptographicHash>
#include "toolfunctions.h"

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
    int getQId() const;
    void setQId(int newQId);

    QString getRetPath() const;
    void setRetPath(const QString &newRetPath);

private slots:
    void on_lineEdit_textChanged(const QString &arg1);
    void on_buttonBox_accepted();
    void onTypstWatcher_standard_output();

private:
    Ui::HtmlTypstAddDialog *ui;
    QProcess typstWatchProcess{this};
    int qId;

    //ret
    QString retContent;
    QString retPath;
};

#endif // HTMLTYPSTADDDIALOG_H
