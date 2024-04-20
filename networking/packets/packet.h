//
// Created by wait4 on 4/7/2024.
//

#ifndef PACKET_H
#define PACKET_H

#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <ostream>

struct Packet {
    int id;
    char buffer[4096];
    int cursor;

    Packet();

    Packet(char* data);

    template <typename T>
    T readNumber();

    template <typename T>
    void writeNumber(T value);

    void WriteString(const std::string& value);

    std::string ReadString();

    void WriteVarInt(int value);

    int ReadVarInt();

    void WriteByteArray(const char* value);

    char* ReadByteArray();

    int GetSize() const;

    void SetCursor(int cursor);

    const Packet Finalize() const;

    const char* Sendable() const;

    friend std::ostream& operator<<(std::ostream& os, const Packet& p) {
        for(int i = 0; i < p.GetSize(); i++) {
            std::stringstream num;
            num << std::setw(4) << std::setfill('0') << i;
            os << "@" << num.str() << " -> " << (int) p.GetBuffer()[i] << "\t(" << p.GetBuffer()[i] << ")" << std::endl;
        }
        return os;
    }
};

#endif //PACKET_H
