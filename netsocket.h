#ifndef NETSOCKET_H
#define NETSOCKET_H
#include <QVector>
#include <QUdpSocket>

class NetSocket : public QUdpSocket
{
    Q_OBJECT

public:
    NetSocket();
    // Bind this socket to a Peerster-specific default port.
    bool bind();
    int getMyPortMin();
    int getMyPortMax();
    int getPort();

    QVector<int> getNeighbors();

private:
    int myPortMin, myPortMax;
    QVector<int> neighbors;
    int port;
};

#endif // NETSOCKET_H
