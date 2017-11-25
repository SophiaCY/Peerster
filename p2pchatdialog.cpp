#include <QGridLayout>
#include <QKeyEvent>
#include <QDebug>
#include "p2pchatdialog.h"


P2PChatDialog::P2PChatDialog(const QString& destination, const QString& host)
{
    chatBox = new QTextEdit(this);
    chatBox->setReadOnly(true);
    inputBox = new QTextEdit(this);
    inputBox->installEventFilter(this);
    QGridLayout* gl = new QGridLayout();
    gl->addWidget(chatBox, 0, 0, 2, -1);
    gl->addWidget(inputBox,2, 0, 1, -1);
    gl->setRowStretch(1, 200);
    gl->setRowStretch(2, 100);
    this->setLayout(gl);
    this->des = destination;
    this->host = host;
    this->setWindowTitle("Chat with " + des);
}

QString P2PChatDialog::getDes()
{
    return this->des;
}

bool P2PChatDialog::eventFilter(QObject *target, QEvent *e)
{
    if (target == inputBox && e->type() == QEvent::KeyPress)
    {
        QKeyEvent *event = static_cast<QKeyEvent *>(e);
        if (event->key() == Qt::Key_Return)
        {
            emit returnPressed(des, inputBox->toPlainText());
            chatBox->append(host + ": " + inputBox->toPlainText());
            inputBox->clear();
            return true;
        }
    }
    return QDialog::eventFilter(target, e);
}

void P2PChatDialog::closeEvent ( QCloseEvent * event )
{
        event->ignore();
        qDebug() << "P2P Dialog has been closed!";
        emit dialogClosed(des);
        event->accept();
}
