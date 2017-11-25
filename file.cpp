#include "file.h"

File::File(const QString& n, quint64 s, const QByteArray& m, const QByteArray& h)
{
    name = n;
    size = s;
    metadata = m;
    hash = h;
}
