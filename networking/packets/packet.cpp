//
// Created by wait4 on 4/7/2024.
//

#include "packet.h"

#include <cstdint>
#include <cstring>

Packet::Packet() {
    std::memset(buffer, 0, sizeof(buffer));
    cursor = 0;
}

Packet::Packet(char* data) {
    std::memcpy(buffer, data, 1024);
    cursor = 0;
}

void Packet::WriteString(const std::string& value) {
    WriteVarInt(value.length());
    std::memcpy(&buffer[cursor], value.c_str(), value.length());
    cursor += value.length();
}

std::string Packet::ReadString() {
    int size = ReadVarInt();
    std::string out(buffer + cursor, size);
    cursor += size;
    return out;
}

void Packet::WriteVarInt(int value) {
    while(true) {
        if((value & ~0x7F) == 0) {
            buffer[cursor++] = value;
            return;
        }
    
        buffer[cursor++] = (value & 0x7F) | 0x80;
   
        value >>= 7;
    }
}

int Packet::ReadVarInt() {
    int value = 0;
    int position = 0;
    char byte = 0;

    while(true) {
        byte = buffer[cursor++];
        value |= (byte & 0x7F) << position;
    
        if((byte & 0x80) == 0) break;
    
        position += 7;
        
        if(position >= 32) return -1;
    }
    
    return value;
}

int Packet::GetSize() const {
    return cursor;
}

void Packet::SetCursor(int cursor) {
    this->cursor = cursor;
}

const char* Packet::Sendable() const {
    return buffer;
}

const Packet Packet::Finalize() const {
    Packet p;
    p.WriteVarInt(cursor);
    std::memcpy(&p.buffer[p.cursor], buffer, cursor);
    p.cursor += cursor;
    return p;
}