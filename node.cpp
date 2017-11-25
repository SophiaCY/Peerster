#include <QDateTime>
#include <QApplication>
#include <QListWidget>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include "netsocket.h"
#include "peer.h"
#include "node.h"
#include "file.h"

Node::Node()
{
    // Create a UDP network socket
    if (!sock.bind())
        exit(1);

    //initialize the seqNo to be 1;
    seqNo = 1;

    qsrand(QDateTime::currentMSecsSinceEpoch() / 1000);
    userName = QHostInfo::localHostName() + "-" + QString::number(sock.getPort()) + "-" + QString::number(qrand() % 1000);

    aETimer.setInterval(10000);
    rrTimer.setInterval(60000);

    //initalize statusMsg
    statusMsg.insert("Want", QVariant(QVariantMap()));

    connect(&sock, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
    aETimer.start();
    connect(&aETimer, SIGNAL(timeout()), this, SLOT(aESendStatusMsg()));
    rrTimer.start();
    connect(&rrTimer, SIGNAL(timeout()), this, SLOT(sendRouteRumor()));

    signalMapper = new QSignalMapper(this);

    forward = true;
    file_id = 0;

    initializeNeighbors();
}

void Node::initializeNeighbors()
{
    QHostInfo info = QHostInfo::fromName(QHostInfo::localHostName());
    myAddress = info.addresses().first();

    qDebug() << myAddress.toIPv4Address() << myAddress.toString() << info.hostName();

    for (int p = sock.getMyPortMin(); p <= sock.getMyPortMax(); p++) {
        if (p == sock.getPort()) {
            continue;
        }
        Peer *peer = new Peer(p, myAddress, info.hostName());
        peers.insert(peer->toString(), peer);
        //peersList->addItem(peer->toString());
    }

    //add host from command line to peer
    QStringList arguments = QCoreApplication::arguments();

    if (arguments.size() > 1) {
        int start = 1;
        if (arguments[1] == "-noforward") {
            forward = false;
            start = 2;
        }
        for (int i = start; i < arguments.size(); i++) {
            QStringList list = arguments[i].split(":");
            QHostInfo peerinfo = QHostInfo::fromName(list[0]);
            QHostAddress address = peerinfo.addresses().first();
            Peer *peer = new Peer(list[1].toInt(), address, peerinfo.hostName());
            peers.insert(peer->toString(), peer);
            qDebug() << "Add peer to peer list";
//            int id = QHostInfo::lookupHost(list[0], this, SLOT(lookedUp(QHostInfo)));
//            lookUp.insert(QString::number(id), list[1].toInt());
        }
    }

    //start up
    sendRouteRumor();
}

Peer* Node::getNeighbor()
{
    int r = qrand() % peers.size();
    QMap<QString,Peer*>::iterator i = peers.begin();
    Peer *p = (i + r).value();
    return p;
}

quint32 Node::getNeighborSize()
{
    return peers.size();
}

QString& Node::getUserName() {
    return userName;
}

void Node::addPeer(const QString& host, quint16 port)
{
    int id = QHostInfo::lookupHost(host, this, SLOT(lookedUp(QHostInfo)));
    lookUp.insert(QString::number(id), port);
}

void Node::lookedUp(const QHostInfo &host)
{
    if (host.error() != QHostInfo::NoError) {
        qDebug() << "Lookup failed:" << host.errorString();
        return;
    }

    QString lookUpId = QString::number(host.lookupId());
    int port = lookUp[lookUpId];
    lookUp.remove(lookUpId);
    QHostAddress address = host.addresses().first();
    QString hn = host.hostName();
    Peer *p = new Peer(port, address, hn);
    QString s = p->toString();

    if (!peers.contains(s)) {
        peers.insert(s, p);
//        emit newPeer(s);
        qDebug() << "Add peer to peer list:" << hn << address.toString() << port;
    }
}

void Node::sendRouteRumor() {
    qDebug() << "sendRouteMsg";
    QVariantMap msg;
    msg.insert("Origin", userName);
    msg.insert("SeqNo", seqNo);
    QString msgId = userName + "_" + QString::number(seqNo);
    msgRepo.insert(msgId, msg);
//    Peer *neighbor = getNeighbor();
//    sendRumorMsg(neighbor->getAddress(), neighbor->getPort(), msgId);
    sendMsgToAllPeers(msg);
    addStatus(userName, seqNo);
    seqNo++;
}

void Node::sendNewMsg(const QString& content)
{
    QVariantMap msg;
    msg.insert("ChatText", content);
    msg.insert("Origin", userName);
    msg.insert("SeqNo", seqNo);
    QString msgId = userName + "_" + QString::number(seqNo);
    msgRepo.insert(msgId, msg);
    Peer *neighbor = getNeighbor();
    sendRumorMsg(neighbor->getAddress(), neighbor->getPort(), msgId);
    addStatus(userName, seqNo);
    seqNo++;
}
void Node::sendP2PMsg(const QString& des, const QString& chatText)
{

    QVariantMap msg;
    msg.insert("Dest", des);
    msg.insert("ChatText", chatText);
    msg.insert("Origin", userName);
    msg.insert("HopLimit", 10);

    QPair<QHostAddress, quint16> pair = routingTable[des];
    sendMsg(pair.first, pair.second, msg);
    qDebug() << "send private msg " << chatText << " to " << pair.first;
}

void Node::sendMsg(const QHostAddress& receiverIP , quint16 receiverPort, const QVariantMap& msg)
{
    QByteArray datagram;
    QDataStream out(&datagram, QIODevice::WriteOnly);
    out << msg;
    sock.writeDatagram(datagram, receiverIP, receiverPort);
}

void Node::sendRumorMsg(const QHostAddress& receiverIP , quint16 receiverPort, const QString& msgId)
{
    QVariantMap msg = msgRepo[msgId].toMap();
    if (forward || !msg.contains("ChatText")) {
     //   qDebug() << "send rumor message " << msgId << " to " << receiverIP << ":" <<receiverPort;
        sendMsg(receiverIP, receiverPort, msg);
    }

    if (!rumorTimers.contains(msgId)) {
        //wait for responding status massage, timer start
        QTimer* timer = new QTimer();
        timer->setInterval(2000);
        rumorTimers.insert(msgId, timer);
        connect(timer, SIGNAL(timeout()), signalMapper, SLOT(map()));
        signalMapper -> setMapping(timer, msgId);
        connect(signalMapper, SIGNAL(mapped(QString)),this, SLOT(continueRumormongering(QString)));
        timer->start();
    }
}


void Node::addStatus(const QString& origin, int n)
{
    QVariantMap map = statusMsg["Want"].toMap();
    map.insert(origin, QVariant(n + 1));
    statusMsg["Want"] = QVariant(map);
   // qDebug() << origin << qvariant_cast<QVariantMap>(statusMsg["Want"])[origin].toString();
}

void Node::sendStatusMsg(const QHostAddress& desAddr, quint16 desPort)
{
  //  qDebug() << "send status message to" << desAddr.toString() << ":" << desPort;
    sendMsg(desAddr, desPort, statusMsg);
}

void Node::aESendStatusMsg()
{
    qDebug() << "Anti-entropy starts";
    Peer *neighbor = getNeighbor();
    sendStatusMsg(neighbor->getAddress(), neighbor->getPort());
}
void Node::readPendingDatagrams()
{
    while (sock.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(sock.pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        sock.readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        if (!peers.contains(sender.toString() + ":" + senderPort)) {
            int id = QHostInfo::lookupHost(sender.toString(), this, SLOT(lookedUp(QHostInfo)));
            lookUp.insert(QString::number(id), senderPort);
        }
        QDataStream in(&datagram, QIODevice::ReadOnly);
        QVariantMap msg;
        in >> msg;
     //   qDebug() << msg;
        if (msg.contains("Search")) {
            processSearchRequest(msg);
        } else if (msg.contains("Dest")) {
            processPrivateMsg(msg);
        } else if (msg.contains("Origin")) {
            processRumorMsg(msg, sender, senderPort);
        } else if (msg.contains("Want")) {
            processStatusMsg(msg, sender, senderPort);
        }
    }
}
void Node::processPrivateMsg(QVariantMap privateMsg)
{
    qDebug() << "PRIV: receive private msg";

    if (privateMsg["Dest"] != userName && privateMsg["HopLimit"].toInt() > 0 && forward) {
        QPair<QHostAddress, quint16> pair = routingTable[privateMsg["Dest"].toString()];
        privateMsg["HopLimit"] = privateMsg["HopLimit"].toInt() - 1;
        sendMsg(pair.first, pair.second, privateMsg);
    } else if (privateMsg["Dest"] == userName) {
        if (privateMsg.contains("BlockRequest")) {
            processBlockRequest(privateMsg);
        } else if (privateMsg.contains("BlockReply")) {
            receiveBlockReply(privateMsg);
        } else if (privateMsg.contains("SearchReply")){
            processSearchReply(privateMsg);
        } else {
             emit newPrivateLog(privateMsg["Origin"].toString(), privateMsg["ChatText"].toString());
        }
    }

}

void Node::processRumorMsg(QVariantMap rumorMsg, const QHostAddress& sender, quint16 senderPort)
{
    int n = rumorMsg["SeqNo"].toInt();
    QString origin = rumorMsg["Origin"].toString();
    QString msgId = origin + "_" + QString::number(n);
    QVariantMap map = statusMsg["Want"].toMap();

    if (rumorMsg.contains("LastIP")) { 
        quint32 lastIP = rumorMsg["LastIP"].toInt();
        QString address= QHostAddress(lastIP).toString();
        QString lastPort = rumorMsg["lastPort"].toString();
        QString lastId = address + ":" + lastPort;
        if (!peers.contains(lastId) && address != myAddress.toString()) {
            int id = QHostInfo::lookupHost(address, this, SLOT(lookedUp(QHostInfo)));
            lookUp.insert(QString::number(id), senderPort);
        }
    }

    //new message

    if ((!map.contains(origin) && n == 1)|| (map.contains(origin) && map[origin].toInt() == n)) {
        QHostAddress sourceIP = sender;
        quint16 sourcePort = senderPort;
        if(rumorMsg.count("LastIP") && rumorMsg.count("LastPort")){
            sourceIP = QHostAddress(rumorMsg["LastIP"].toInt());
            sourcePort = (quint16)rumorMsg["LastPort"].toInt();
        }
        rumorMsg.insert("LastIP", sender.toIPv4Address());
        rumorMsg.insert("LastPort", senderPort);

        if (rumorMsg.contains("ChatText")) {
            qDebug() << "CHAT RUMOR: receive chat rumor from " << origin << ": " << rumorMsg["ChatText"].toString();
            emit newLog(origin + ": " + rumorMsg["ChatText"].toString());
                //send the new rumor to its neighbour
            continueRumormongering(msgId, sender, senderPort);

        } else { //route rumor message
            qDebug() << "ROUTE RUMOR: receive route rumor from " << sender.toString() << ": " << msgId;
            sendMsgToAllPeers(rumorMsg);
        }
        //add rumor msg to the map
        msgRepo.insert(msgId, QVariant(rumorMsg));

        addStatus(origin, n);

        if (!routingTable.contains(origin)) {
            emit newPeer(origin);
            qDebug() << "Add peer to routing table:" << origin;
            sendRouteRumor();
        }
        routingTable.insert (origin, QPair<QHostAddress, quint16>(sourceIP, sourcePort));
    //    qDebug() << "routingTable insert" << origin << " " << sender;
    } else if (map.contains(origin) && map[origin].toInt() - 1 == n) {
         if(!rumorMsg.contains("LastIP")) {
         //    qDebug() << "receive direct msg from" << sender.toString() << ": " << msgId;;
             routingTable.insert(origin, QPair<QHostAddress, quint16>(sender, senderPort));
         }
     }

    //send status msg to the sender
    sendStatusMsg(sender, senderPort);

}

void Node::continueRumormongering(const QString& msgId)
{
    Peer *receiver = getNeighbor();
    sendRumorMsg(receiver->getAddress(), receiver->getPort(), msgId);
}

void Node::continueRumormongering(const QString& msgId, const QHostAddress& add, quint16 port)
{
    Peer *receiver;
    do {
       receiver = getNeighbor();
    } while(receiver->getAddress() == add && receiver->getPort() == port);

    sendRumorMsg(receiver->getAddress(), receiver->getPort(), msgId);
}

void Node::sendMsgToAllPeers(const QVariantMap& msg)
{
    qDebug() << "send route msg to all peers";
    for (Peer* p: peers.values()) {
        sendMsg(p->getAddress(), p->getPort(), msg);
    }
}

void Node::processStatusMsg(QVariantMap senderStatusMsg, const QHostAddress& sender, quint16 senderPort)
{
    QString senderName = sender.toString() + ":" + QString::number(senderPort);
//    qDebug() << "STATUS: receive status message from" << senderName;

    QVariantMap::Iterator itr;
    QVariantMap status = statusMsg["Want"].toMap();
    QVariantMap senderStatus = senderStatusMsg["Want"].toMap();
    //qDebug() << "My Status:";
    for (itr = status.begin(); itr != status.end(); itr++) {
        QString origin = itr.key();
     //  qDebug() << origin << status[origin].toInt();
        if (senderStatus.contains(origin)) {
            int senderSeqNo = senderStatus[origin].toInt();
            int statusSeqNo = status[origin].toInt();
            if (senderSeqNo > statusSeqNo) {
                //send a status msg to the sender
                sendStatusMsg(sender, senderPort);
                return;
            } else if (senderSeqNo < statusSeqNo){
                //send the corresponding rumor msg
                QString msgId = origin + "_" + QString::number(senderSeqNo);
                sendRumorMsg(sender, senderPort, msgId);
                return;
            }
        } else {
            QString msgId = origin + "_1";
            sendRumorMsg(sender, senderPort, msgId);
            return;
        }
    }
    //qDebug() << "Sender status:";
    for (itr = senderStatus.begin(); itr != senderStatus.end(); itr++) {
     //   qDebug() << itr.key() << senderStatus[itr.key()].toInt();
        QString origin = itr.key();
        int senderSeqNo = senderStatus[origin].toInt();
        QString msgId = origin + "_" + QString::number(senderSeqNo - 1);
        if (!status.contains(origin)) {
            sendStatusMsg(sender, senderPort);
            return;
        } else if (rumorTimers.contains(msgId)) {
            if (qrand() % 2) {
                qDebug() << "stop timer for msg" << msgId;
                //1, head, send rumor
                rumorTimers[msgId]->stop();
                delete rumorTimers[msgId];
                rumorTimers.remove(msgId);
            }
        }
    }

}

void Node::processBlockRequest(const QVariantMap& request)
{
    qDebug() << "BLOCK REQ: receive block request";
    if (!request.contains("BlockRequest") || !request.contains("Origin")) {
        qDebug() << "Format Error!";
        qDebug() << request;
        return;
    }
    QString hash = QCA::arrayToHex(request["BlockRequest"].toByteArray());
    QByteArray data;
    if (metadatas.contains(hash)) {
        data = metadatas[hash];
    } else if (hashToFile.contains(hash)) {
        QString filepath = hashToFile[hash].first;
        int blockid = hashToFile[hash].second;
        data = readBlockData(filepath, blockid);
    } else {
        return;
    }
    QVariantMap reply;
    reply.insert("Dest", request["Origin"].toString());
    reply.insert("Origin", userName);
    reply.insert("HopLimit", 10);
    reply.insert("BlockReply", request["BlockRequest"]);
    reply.insert("Data", data);

    QPair<QHostAddress, quint16> pair = routingTable[request["Origin"].toString()];
    sendMsg(pair.first, pair.second, reply);

}

void Node::receiveBlockReply(const QVariantMap& reply)
{
    qDebug() << "BLOCK REPlY: receive block reply";
    if (!reply.contains("BlockReply") || !reply.contains("Data") || !reply.contains("Origin")) {
        qDebug() << "Format Error!";
        qDebug() << reply;
        return;
    }
    QByteArray hash = reply["BlockReply"].toByteArray();
    QString hashHex = QCA::arrayToHex(hash);
    QByteArray data = reply["Data"].toByteArray();
    QString origin = reply["Origin"].toString();
    QCA::Hash shaHash("sha1");
    shaHash.update(data);
    QByteArray hashResult = shaHash.final().toByteArray();
    if (hashHex != QCA::arrayToHex(hashResult)) {
        qDebug() << "not equal return";
        qDebug() << hashHex;
        qDebug() << QCA::arrayToHex(hashResult);
        return;
    }
    if (metaFileRequests.contains(hashHex)) {
        //receive a metadata
        qDebug() << "received data hash hex: " << QCA::arrayToHex(hashResult);
        int size = data.size();
        QByteArray ba1 = data.mid(0, 20);
        QByteArray ba2;
        if (size > 20) {
            ba2 += data.mid(20);
        }
        sendBlockRequest(origin, ba1);
        QString fileName = metaFileRequests[hashHex];
        QDir dir(DOWNLOAD_FILES_DIR);
        if (!dir.exists()) {
            QDir().mkdir(DOWNLOAD_FILES_DIR);
        }
        if (fileName == "") {
             fileName = DOWNLOAD_FILES_DIR + "/file_" + QString::number(file_id) + ".dn";
             file_id++;
        }
        QFile* f = new QFile(fileName);
        f->open(QFile::WriteOnly|QFile::Truncate);
        f->close();
        blockRequests[ba1] = QPair<QFile*, QByteArray>(f, ba2);
        metaFileRequests.remove(hashHex);
    } else if (blockRequests.contains(hash)){
        //receive a data block
        qDebug() << "receive Data Block";
        QFile* file = blockRequests[hash].first;
        write(file, data);
        QByteArray ba = blockRequests[hash].second;
        if (ba.size() != 0) {
            QByteArray ba1 = ba.mid(0, 20);
            QByteArray ba2;
            if (ba.size() > 20) {
                ba2 += ba.mid(20);
            }
            blockRequests[ba1] = QPair<QFile*, QByteArray>(file, ba2);
            sendBlockRequest(origin, ba1);
        } else {
            delete file;
        }
        blockRequests.remove(hash);
    }
}

QByteArray Node::readBlockData(QString& filename, int id)
{
    QFile file(filename);
    QByteArray data;
    if (!file.open(QIODevice::ReadOnly))
        return data;
    int count = 0;
    while (count <= id) {
        data = file.read(BLOCKSIZE);
        count++;
    }
    return data;
}

void Node::shareFile(const QString& filepath)
{
   QFile file(filepath);
   QFileInfo fileInfo(filepath);
   QString filename(fileInfo.fileName());
   if (!file.open(QIODevice::ReadOnly))
       return;
   QByteArray bhash;
   int i = 0;
   while (!file.atEnd()) {
       QByteArray data = file.read(BLOCKSIZE);
       QCA::Hash shaHash("sha1");
       shaHash.update(data);
       QByteArray hashResult = shaHash.final().toByteArray();
       hashToFile.insert(QCA::arrayToHex(hashResult), QPair<QString, int>(filepath, i));
       bhash += hashResult;
       i++;
   }

 //  qDebug() << hash;
 //  QString mfpath = META_FILES_DIR + filename.split(".")[0] + QString::number(qrand() % 1000) + ".mt";
 //  qDebug() << "metafile: " << mfpath;
 //  qDebug() << i;
   qDebug() << "hash size:" << bhash.size();
 //  writeMeta(mfpath, bhash);
   QCA::Hash shaHash("sha1");
   shaHash.update(bhash);
   QByteArray hashResult = shaHash.final().toByteArray();
   QString fid = QCA::arrayToHex(hashResult);
   qDebug() << "Hash result:" << fid;
   File *mf = new File(filename, file.size(), bhash, hashResult);
   metafiles[filename] = mf;
   metadatas.insert(fid, bhash);
 //  hashToFile.insert(fid, QPair<QString, int>(mfpath, 0));
}
void Node::download(const QString& nodeId, const QString& hash)
{
    sendBlockRequest(nodeId, QCA::hexToArray(hash));
    metaFileRequests[hash] = "";
}

void Node::sendBlockRequest(const QString& nodeId, const QByteArray& hash)
{
    QVariantMap msg;
    msg.insert("Dest", nodeId);
    msg.insert("Origin", userName);
    msg.insert("HopLimit", 10);
    msg.insert("BlockRequest", hash);

    QPair<QHostAddress, quint16> pair = routingTable[nodeId];
    sendMsg(pair.first, pair.second, msg);

    qDebug() << "send block request to " << pair.first;
    qDebug() << "block request hash hex:" << QCA::arrayToHex(hash);
}

void Node::write(QFile* f, const QByteArray& data)
{
    if (!f->open(QIODevice::WriteOnly | QIODevice::Append)) {
        return;
    }
    f->write(data);
    qDebug() << f->pos();
    f->close();
}

void Node::writeMeta(const QString& filename, const QByteArray& data)
{
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly)) {
        return;
    }
    QTextStream out(&f);
    out << data;
}

void Node::initSearch(const QString keywords)
{
    qDebug() << "Search keywords: " << keywords;
    Peer *neighbor = getNeighbor();
    sendSearchReq(userName, keywords, 2, neighbor->getAddress(), neighbor->getPort());
    QTimer* timer = new QTimer();
    timer->setInterval(1000);
    searchMatches.insert(keywords, QPair<quint32, quint32>(2, 0));
    searchTimers.insert(keywords, timer);
    connect(timer, SIGNAL(timeout()), signalMapper, SLOT(map()));
    signalMapper -> setMapping(timer, keywords);
    connect(signalMapper, SIGNAL(mapped(QString)),this, SLOT(continueSearch(QString)));
    timer->start();
}

void Node::sendSearchReq(const QString& origin, const QString keywords, quint32 budget, const QHostAddress add, quint16 port)
{
    QVariantMap msg;
    msg.insert("Origin", origin);
    msg.insert("Search", keywords);
    msg.insert("Budget", budget);

    sendMsg(add, port, msg);

}

void Node::continueSearch(const QString& keywords) {
    if (!searchTimers.contains(keywords) || !searchMatches.contains(keywords)) {
        return;
    }
    QTimer* timer = searchTimers[keywords];
    QPair<quint32, quint32> pair = searchMatches[keywords];
    quint32 curBudget = pair.first;
    quint32 curMatch = pair.second;
    if (curBudget >= 100 || curMatch >= 10) {
        timer->stop();
        delete timer;
        searchTimers.remove(keywords);
        searchMatches.remove(keywords);
        return;
    }
    Peer *neighbor = getNeighbor();

    sendSearchReq(userName, keywords, curBudget * 2, neighbor->getAddress(), neighbor->getPort());

    searchTimers.insert(keywords, timer);
    searchMatches.insert(keywords, QPair<quint32, quint32>(curBudget * 2, curMatch));
}

void Node::processSearchRequest(const QVariantMap& req) {
    qDebug() << "Receive search request!";
    if (!req.contains("Search") || !req.contains("Budget") || !req.contains("Origin")) {
        qDebug() << "Format Error!";
        qDebug() << req;
        return;
    }
    QString origin = req["Origin"].toString();
    QString keywords = req["Search"].toString();
    QStringList keywordList = keywords.split(" ");
    quint32 budget = req["Budget"].toInt();
    if (origin == userName) {
        return;
    }
    budget--;
    if (keywordList.size() == 0) {
        keywordList.append(keywords);
    }

    for (int i = 0; i < keywordList.size(); i++) {
        QVariantList matchNames;
        QVariantList matchIds;
        for (auto itr = metafiles.begin(); itr != metafiles.end(); itr++) {
            QString filename = itr.key();
            if (filename.contains(keywordList[i], Qt::CaseInsensitive)) {
                matchNames.append(filename);
                File *mf = itr.value();
                matchIds.append(mf->hash);
            }
        }

        sendSearchReply(keywordList[i], matchNames, matchIds, origin);
    }

    if (budget > 0) {
        redistribute(origin, keywords, budget);
    }
}

void Node::sendSearchReply(const QString& reply, const QVariantList& matchNames, const QVariantList& matchIds, const QString& des) {
    QVariantMap msg;
    msg.insert("Dest", des);
    msg.insert("Origin", userName);
    msg.insert("HopLimit", 10);
    msg.insert("SearchReply", reply);
    msg.insert("MatchNames", matchNames);
    msg.insert("MatchIDs", matchIds);

    QPair<QHostAddress, quint16> pair = routingTable[des];
    sendMsg(pair.first, pair.second, msg);
}

void Node::redistribute(const QString& origin, const QString& keywords, quint32 budget)
{
    quint32 neighborSize = getNeighborSize();
    auto itr = peers.begin();
    if (neighborSize >= budget) {
        QVector<int> seq = shuffle(neighborSize);
        for (quint32 i = 0; i < budget; i++) {
            Peer* neighbor = (itr + seq[i]).value();
            sendSearchReq(origin, keywords, 1, neighbor->getAddress(), neighbor->getPort());
        }
    } else {
        quint32 avg_b = budget / neighborSize;
        quint32 r = budget % neighborSize;
        for (; itr != peers.end(); itr++) {
            quint32 b = r <= 0? avg_b: avg_b + 1;
            r--;
            sendSearchReq(origin, keywords, b, itr.value()->getAddress(), itr.value()->getPort());
        }
    }
}

QVector<int> Node::shuffle(int n)
{
    QVector<int> res;
    for (int i = 0; i < n; i++) {
        res.push_back(i);
    }
    for (int i = 2; i < n; i++) {
        int tmp = res[i];
        int j = qrand() % n;
        res[i] = res[j];
        res[j] = tmp;
    }
    return res;
}

void Node::processSearchReply(const QVariantMap& reply)
{
    qDebug() << "Receive search reply!";
    if (!reply.contains("Origin") || !reply.contains("MatchNames") || !reply.contains("MatchIDs")) {
        qDebug() << "Format Error!";
        qDebug() << reply;
        return;
    }
    QString origin = reply["Origin"].toString();
    QVariantList matchNames = reply["MatchNames"].toList();
    QVariantList matchIds = reply["MatchIDs"].toList();
    for (int i = 0; i < matchNames.size(); i++) {
        QString filename = matchNames[i].toString();
        qDebug() << filename;
        QString source = origin + ":"+ filename;
        if (searchReplies.contains(source)) {
            continue;
        }
        QByteArray hash = matchIds[i].toByteArray();
        searchReplies.insert(source, QPair<QString, QByteArray>(origin, hash));
        emit newSearchRes(source);
    }
}

void Node::downloadSearchedFile(const QString& fileId)
{
    QPair<QString, QByteArray> p = searchReplies[fileId];
    qDebug() << "selected File: " << fileId;
    sendBlockRequest(p.first, p.second);
    metaFileRequests[QCA::arrayToHex(p.second)] = DOWNLOAD_FILES_DIR + "/" + fileId.split(":")[1];
}


