#include "operatorsignals.h"

// Overload << so that std::bitset can be serialised
QDataStream& operator<<(QDataStream& out, const std::bitset<1>& bitset) {
    quint8 value = bitset.to_ulong(); // bitset<1> converts directly to unsigned long
    out << value;
    return out;
}

// Overload >> so that std::bitset can be deserialised
QDataStream& operator>>(QDataStream& in, std::bitset<1>& bitset) {
    quint8 value;
    in >> value;
    bitset = std::bitset<1>(value);
    return in;
}
