#include <QApplication>
#include <QVBoxLayout>
#include <QDebug>
#include <QKeyEvent>
#include <QVector>
#include <QFormLayout>
#include <QFileDialog>
#include "p2pchatdialog.h"
#include "main.hh"

ChatDialog::ChatDialog()
{
	setWindowTitle("Peerster");

    QLabel* label = new QLabel("hostname:port/ipaddr:port :", this);
    peerInfo = new QLineEdit(this);
    addButton = new QPushButton("Add Peer",this);
//    peerListLabel = new QLabel("Peer List:");
//    peerList = new QTextEdit(this);
//    peerList->setReadOnly(true);
    QLabel* peerLabel = new QLabel("Online Peers:");
    onlinePeers = new QListWidget(this);

    QLabel* flable = new QLabel("Shared Files:", this);
    sharedFiles = new QListWidget(this);
    shareFileBtn = new QPushButton("Share Files", this);
    QLabel* nodeIdLabel = new QLabel("Node Id:");
    nodeIdLabel->setFixedWidth(55);
    nodeId = new QLineEdit(this);
    QLabel* l = new QLabel("Type the node id or select from the left by single click.");
    QLabel* fileIdLabel = new QLabel("File Id:");
    fileIdLabel->setFixedWidth(55);
    fileId = new QLineEdit(this);
    downloadBtn = new QPushButton("Download File", this);

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//
	// You might change this into a read/write QTextEdit,
	// so that the user can easily enter multi-line messages.
	textline = new QTextEdit(this);
	textline->setFocus();
	textline->installEventFilter(this);


    QLabel* searchResultLabel = new QLabel("Search Results:", this);
    fileLists = new QListWidget(this);
    fileLists->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QLabel* keywordLabel = new QLabel("Keywords:", this);
    keywords = new QLineEdit(this);
    searchBtn = new QPushButton("Search", this);
	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
    QGridLayout *layout1 = new QGridLayout();

    layout1->addWidget(textview, 0, 0, 2, -1);
    layout1->addWidget(textline, 2, 0, 1, -1);
    layout1->setRowStretch(1, 200);
    layout1->setRowStretch(2, 100);

    QVBoxLayout *layout2 = new QVBoxLayout();
    layout2->addWidget(label);
    layout2->addWidget(peerInfo);
    layout2->addWidget(addButton);
    layout2->addWidget(peerLabel);
    layout2->addWidget(onlinePeers);

    QVBoxLayout *layout3 = new QVBoxLayout();
    layout3->addWidget(flable);
    layout3->addWidget(sharedFiles);
    layout3->addWidget(shareFileBtn);

    QHBoxLayout *layout3_1 = new QHBoxLayout();
    layout3_1->addWidget(nodeIdLabel);
    layout3_1->addWidget(nodeId);
    QHBoxLayout *layout3_2 = new QHBoxLayout();
    layout3_2->addWidget(fileIdLabel);
    layout3_2->addWidget(fileId);

    layout3->addWidget(l);
    layout3->addLayout(layout3_1);
    layout3->addLayout(layout3_2);
    layout3->addWidget(downloadBtn);

    QVBoxLayout *layout4 = new QVBoxLayout();
    layout4->addWidget(searchResultLabel);
    layout4->addWidget(fileLists);
    layout4->addWidget(keywordLabel);
    layout4->addWidget(keywords);
    layout4->addWidget(searchBtn);


    QHBoxLayout *globalLayout = new QHBoxLayout();
    globalLayout->addLayout(layout1);
    globalLayout->addLayout(layout2);
    globalLayout->addLayout(layout3);
    globalLayout->addLayout(layout4);
    setLayout(globalLayout);

    mapper = new QSignalMapper();

    node = new Node();

    connect(addButton, SIGNAL(clicked()), this, SLOT(addPeerButtonClicked()));
    connect(shareFileBtn, SIGNAL(clicked()), this, SLOT(shareFileButtonClicked()));
    connect(downloadBtn, SIGNAL(clicked(bool)), this, SLOT(downloadFile()));
    connect(node, SIGNAL(newLog(QString)), this, SLOT(appendLog(QString)));
    connect(node, SIGNAL(newPeer(QString)), this, SLOT(addPeer(QString)));
    connect(onlinePeers, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(selectNode(QListWidgetItem*)));
    connect(onlinePeers, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(newDialog(QListWidgetItem*)));
    connect(node, SIGNAL(newPrivateLog(QString,QString)), this, SLOT(receiveNewPrivLog(QString, QString)));
    connect(searchBtn, SIGNAL(clicked(bool)), this, SLOT(search()));
    connect(node, SIGNAL(newSearchRes(QString)), this, SLOT(addSearchRes(QString)));
    connect(fileLists, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(downloadSearchedFile(QListWidgetItem*)));
}


void ChatDialog::newDialog(QListWidgetItem* item) {
    P2PChatDialog *p2pDialog = new P2PChatDialog(item->text(), node->getUserName());
    p2pDialog->show();
    peerDialogMap.insert(item->text(), p2pDialog);
    connect(p2pDialog, SIGNAL(returnPressed(QString, QString)), this, SLOT(sendPrivMsg(QString, QString)));
    connect(p2pDialog, SIGNAL(dialogClosed(QString)), SLOT(p2pdialogClosed(QString)));
}

void ChatDialog::p2pdialogClosed(const QString& item) {
    peerDialogMap.remove(item);
}

bool ChatDialog::eventFilter(QObject *target, QEvent *e)
{
    if (target == textline && e->type() == QEvent::KeyPress)
    {
        QKeyEvent *event = static_cast<QKeyEvent *>(e);
        if (event->key() == Qt::Key_Return)
        {
            gotReturnPressed();
            return true;
        }
    }
    return QDialog::eventFilter(target, e);
}

void ChatDialog::addPeerButtonClicked()
{
    if (peerInfo->text() == "") {
        return;
    }
    QStringList list = peerInfo->text().split(":");
    if (list.length() == 0) {
        return;
    }
    QString host = list[0];
    QString port = list[1];
    peerInfo->clear();
    node->addPeer(host, port.toInt());
//    QHostAddress ip = QHostAddress(host);
//    if (ip.isNull()) {//host is a hostname

}



void ChatDialog::shareFileButtonClicked()
{
    QStringList filenames = QFileDialog::getOpenFileNames(this, tr("Select Files"), QDir::currentPath(), tr("all files (*)"));
    if (!filenames.isEmpty())
    {
        for (int i = 0; i < filenames.count(); i++) {
            sharedFiles->addItem(filenames.at(i));
            node->shareFile(filenames.at(i));
        }
    }
}

void ChatDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...

    node->sendNewMsg(textline->toPlainText());
    textview->append(node->getUserName() + ": " + textline->toPlainText());
	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::sendPrivMsg(const QString& des, const QString& content) {
    node->sendP2PMsg(des, content);
}


void ChatDialog::appendLog(const QString& text)
{
    textview->append(text);
}

void ChatDialog::receiveNewPrivLog(const QString& origin, const QString& text)
{
    if (!peerDialogMap.contains(origin)) {
        QList<QListWidgetItem*> items = onlinePeers->findItems(origin, Qt::MatchExactly);
        if (items.size() == 0) {
            return;
        }
        newDialog(items[0]);
    }
    peerDialogMap[origin]->chatBox->append(origin + ": " + text);
}

void ChatDialog::addPeer(const QString& s)
{
    onlinePeers->addItem(s);
}

void ChatDialog::closeEvent (QCloseEvent * event)
{
    event->accept();
    exit(0);
}

void ChatDialog::selectNode(QListWidgetItem *item)
{
    nodeId->insert(item->text());
}

void ChatDialog::downloadFile()
{
    node->download(nodeId->text(), fileId->text());
    nodeId->clear();
    fileId->clear();
}

void ChatDialog::search()
{
    node->initSearch(keywords->text());
    keywords->clear();
}

void ChatDialog::addSearchRes(const QString &res)
{
    fileLists->addItem(res);
}

void ChatDialog::downloadSearchedFile(QListWidgetItem *item)
{
    node->downloadSearchedFile(item->text());
}

int main(int argc, char **argv)
{
    QCA::Initializer qcainit;
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
	ChatDialog dialog;
	dialog.show();
	
	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}
