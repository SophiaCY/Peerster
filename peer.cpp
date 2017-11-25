#include "peer.h"

Peer::Peer(quint16 p, const QHostAddress& a, const QString& s)
{
    port = p;
    address = a;
    hostname = s;
}

QString Peer::toString()
{
    return address.toString() + ":" + QString::number(port);
}

quint16 Peer::getPort()
{
    return port;
}

QHostAddress Peer::getAddress()
{
    return address;
}

QString Peer::getHostName()
{
    return hostname;
}
