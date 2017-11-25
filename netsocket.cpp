#include <unistd.h>
#include "netsocket.h"

NetSocket::NetSocket()
{
    // Pick a range of four UDP ports to try to allocate by default,
    // computed based on my Unix user ID.
    // This makes it trivial for up to four Peerster instances per user
    // to find each other on the same host,
    // barring UDP port conflicts with other applications
    // (which are quite possible).
    // We use the range from 32768 to 49151 for this purpose.
    myPortMin = 32768 + (getuid() % 4096)*4;
    myPortMax = myPortMin + 3;

}

bool NetSocket::bind()
{
    // Try to bind to each of the range myPortMin..myPortMax in turn.
    for (int p = myPortMin; p <= myPortMax; p++) {
        if (QUdpSocket::bind(p)) {
            port = p;
            qDebug() << "bound to UDP port " << p;
            if (p + 1 <= myPortMax) {
                neighbors.append(p + 1);
            }
            if (p - 1 >= myPortMin) {
                neighbors.append(p - 1);
            }
            return true;
        }
    }

    qDebug() << "Oops, no ports in my default range " << myPortMin
        << "-" << myPortMax << " available";
    return false;
}

int NetSocket::getMyPortMin() {
    return myPortMin;
}

int NetSocket::getMyPortMax() {
    return myPortMax;
}
int NetSocket::getPort() {
    return port;
}
QVector<int> NetSocket::getNeighbors() {
    return neighbors;
}
