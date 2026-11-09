#pragma once

#include <QDataStream>
#include <bitset>

// Overload << so that std::bitset can be serialised
QDataStream& operator<<(QDataStream& out, const std::bitset<1>& bitset);

// Overload >> so that std::bitset can be deserialised
QDataStream& operator>>(QDataStream& in, std::bitset<1>& bitset);
