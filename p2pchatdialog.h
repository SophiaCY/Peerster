#ifndef P2PCHATDIALOG_H
#define P2PCHATDIALOG_H

#include <QTextEdit>
#include <QDialog>

class P2PChatDialog : public QDialog
{
    Q_OBJECT

public:
    P2PChatDialog(const QString& des,  const QString& host);
    QString getDes();
    QTextEdit* chatBox;
    QTextEdit* inputBox;

signals:
    void returnPressed(const QString& des, const QString& content);
    void dialogClosed(const QString& item);

protected:
    bool eventFilter(QObject *obj, QEvent *e);
    void closeEvent ( QCloseEvent * event);

private:
    QString des;
    QString host;
};


#endif // P2PCHATDIALOG_H
