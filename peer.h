#ifndef PEER_H
#define PEER_H
#include <QString>
#include <QHostAddress>

class Peer : public QObject {

    Q_OBJECT

public:
    Peer();
    Peer(quint16 p, const QHostAddress& a, const QString& s);
    QString toString();
    quint16 getPort();
    QHostAddress getAddress();
    QString getHostName();

private:
    quint16 port;
    QHostAddress address;
    QString hostname;

};

#endif // PEER_H
