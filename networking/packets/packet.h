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
    char buffer[65536];
    int cursor;

    Packet();

    Packet(char* data);

    template <typename T>
    T readNumber() {
        T value;
        std::memcpy(&value, &buffer[cursor], sizeof(T));
        cursor += sizeof(T);
        return value;
    }

    template <typename T>
    void writeNumber(T value) {
        std::memcpy(&buffer[cursor], &value, sizeof(T));
        cursor += sizeof(T);
    }

    void WriteString(const std::string& value);

    std::string ReadString();

    void WriteVarInt(int value);

    int ReadVarInt();

    template <typename T>
    void writeArray(const T* value, int length) {
        std::memcpy(&buffer[cursor], &value, length * sizeof(T));
        cursor += length * sizeof(T);
    }

    template <typename T>
    T* readArray(int length) {
        T* value;
        std::memcpy(&value, &buffer[cursor], length * sizeof(T));
        cursor += length * sizeof(T);
        return value;
    }

    int GetSize() const;

    void SetCursor(int cursor);

    const Packet Finalize() const;

    const char* Sendable() const;

    friend std::ostream& operator<<(std::ostream& os, const Packet& p) {
        for(int i = 0; i < p.GetSize(); i++) {
            std::stringstream num;
            num << std::setw(4) << std::setfill('0') << i;
            os << "@" << num.str() << " -> 0x" << std::hex << std::setw(2) << std::setfill('0') << (((int) p.Sendable()[i]) & 0xFF);
            os << "\t(" << p.Sendable()[i] << ")" << std::endl;
        }
        return os;
    }
};

#endif //PACKET_H
