#include "client.h"
#include "client.h"
//
// Created by wait4 on 4/7/2024.
//

#include "client.h"
#include "../networking/packets/packet.h"
#include <iostream>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#endif

void Player::formatSocket() {
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(getSocket(), FIONBIO, &mode) == SOCKET_ERROR) {
        logger.error("Couldn't set the socket to non-blocking mode!");
        connect = false;
        return;
    }
#else
    int flags = fcntl(getSocket(), F_GETFL, 0);
    if (flags == -1) {
        logger.error("Couldn't set the socket to non-blocking mode!");
        connect = false;
        return;
    }
    flags |= O_NONBLOCK;
    if (fcntl(getSocket(), F_SETFL, flags) == -1) {
        logger.error("Couldn't set the socket to non-blocking mode!");
        connect = false;
        return;
    }
#endif
}

Player::~Player() {
#ifdef WIN32
    closesocket(player.getSocket());
#else
    close(player.getSocket());
#endif
}

void Player::sendPacket(const Packet& final) {
    send(getSocket(), final.Sendable(), final.GetSize(), 0);
}

Packet Player::recievePacket() {
    char buffer[4096];
    const int bytesRecieved = recv(getSocket(), buffer, sizeof(buffer), 0);
    if (bytesRecieved < 0) return Packet();
    if (bytesRecieved == 0) {
        kick({ "Invalid packet data recieved!" });
    }

    Packet in(buffer);
    return in;
}

void Player::tick() {
    Packet p = recievePacket();
    if (p.GetSize() <= 0) return;

    int length = p.ReadVarInt();
    int id = p.ReadVarInt();

    try {
        Player::packetHandlers.at(state).at(id)(p);
    }
    catch (const std::out_of_range& exception) {
        kick({"Invalid player state or packet id!"});
    }
    
}

void Player::kick(TextComponent reason) {
    Packet out;
    switch (state) {
        case CONFIGURATION: {
            out.WriteByte(0x01);
            break;
        }
        case LOGIN:
            out.WriteByte(0x00);
            break;
        case PLAY:
            out.WriteByte(0x1A);
            break;
        default:
            connected = false;
            return;
    }
    out.WriteString(reason.asString());
    sendPacket(out.Finalize());
    connected = false;
}

void Player::keepConnection() {
    return connected;
}

/*
    Handshake Handler Functions
*/

void Player::handleHandshake(Packet& in) {
    int version = in.ReadVarInt();
    std::string addr = in.ReadString();
    unsigned short port = in.ReadUnsignedShort();
    int state = in.ReadVarInt();

    if (state == 1)
        setState(STATUS);
    else if (state == 2)
        setState(LOGIN);
}

void Player::handleLegacyPing(Packet& in) {
    // TODO: handle this bullshit
}

/*
    Status Request Handle Functions
*/

void Player::handleStatusRequest(Packet& in) {

}

void 