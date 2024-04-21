#include "client.h"
#include "client.h"
//
// Created by wait4 on 4/7/2024.
//

#include "client.h"
#include "../networking/packets/packet.h"
#include "../io/logger.h"
#include <iostream>
#include <vector>
#include "minecraft_server.h"
#include <functional>
#include <unordered_map>
#include <map>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#endif

std::unordered_map<ClientState, std::unordered_map<char, HandlerPointer>> generateMap() {
    std::unordered_map<ClientState, std::unordered_map<char, HandlerPointer>> map;

    std::unordered_map<char, HandlerPointer> handshake;
    handshake[0x00] = &Player::handleHandshake;
    map[HANDSHAKE] = handshake;

    std::unordered_map<char, HandlerPointer> status;
    status[0x00] = &Player::handleStatusRequest;
    status[0x01] = &Player::handlePingRequest;
    map[STATUS] = status;

    std::unordered_map<char, HandlerPointer> login;
    login[0x00] = &Player::handleLoginStart;
    login[0x01] = &Player::handleEncryptionResponse;
    login[0x02] = &Player::handlePluginResponse;
    login[0x03] = &Player::handleLoginAcknowledged;
    map[LOGIN] = login;

    std::unordered_map<char, HandlerPointer> config;
    config[0x00] = &Player::handleInformation;
    config[0x01] = &Player::handlePluginMessage;
    config[0x02] = &Player::handleAcknowledgeConfigFinish;
    config[0x03] = &Player::handleKeepAlive;
    config[0x04] = &Player::handlePong;
    config[0x05] = &Player::handleResourcePackResponse;
    map[PLAY] = config;

    return map;
}

const std::unordered_map<ClientState, std::unordered_map<char, HandlerPointer>> Player::packetHandlers = generateMap();

void Player::formatSocket() {
    Logger logger = MinecraftServer::get().getLogger();
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(getSocket(), FIONBIO, &mode) == SOCKET_ERROR) {
        logger.error("Couldn't set the socket to non-blocking mode!");
        connected = false;
        return;
    }
#else
    int flags = fcntl(getSocket(), F_GETFL, 0);
    if (flags == -1) {
        logger.error("Couldn't set the socket to non-blocking mode!");
        connected = false;
        return;
    }
    flags |= O_NONBLOCK;
    if (fcntl(getSocket(), F_SETFL, flags) == -1) {
        logger.error("Couldn't set the socket to non-blocking mode!");
        connected = false;
        return;
    }
#endif
}

Player::~Player() {
    MinecraftServer::get().getLogger().info("Client disconnected.");
#ifdef WIN32
    closesocket(getSocket());
#else
    close(getSocket());
#endif
}

void Player::sendPacket(const Packet& in) {
    Packet f = in.Finalize();
    send(getSocket(), f.Sendable(), f.GetSize(), 0);
}

Packet Player::recievePacket() {
    char buffer[4096];
    const int bytesRecieved = recv(getSocket(), buffer, sizeof(buffer), 0);
    if (bytesRecieved < 0) {
#ifdef WIN32
        if (WSAGetLastError() == WSAECONNRESET) {
#else
        if (errno == ECONNRESET) {
#endif
            MinecraftServer::get().getLogger().info("Connection reset.");
            connected = false;
            return Packet();
        } else {
            return Packet();
        }
    }
    if (bytesRecieved == 0) {
        kick({ "Invalid packet data recieved!" });
    }

    Packet in(buffer);
    return in;
}

void Player::tick() {
    if (!connected) return;
    Packet p = recievePacket();
    if (p.GetSize() > 0) {
        int length = p.ReadVarInt();
        int id = p.ReadVarInt();

        try {
            auto& handler = Player::packetHandlers.at(state).at(id);
            (this->*handler)(p);
        } catch (const std::out_of_range& exception) {
            kick({"Invalid player state or packet id!"});
        }
    }
    
    if (state != HANDSHAKE && state != STATUS) {
        ticksSinceLastKeepAlive++;
        if (lastKeepAlive != 0) {
            if (ticksSinceLastKeepAlive > 300) // TODO: replace constant with config variable
                kick({"Timed out"});
        } else {
            if (ticksSinceLastKeepAlive > 200)
                sendKeepAlive();
        }
    }
}

void Player::kick(TextComponent reason) {
    connected = false;
    Packet out;
    switch (state) {
        case CONFIGURATION: {
            out.writeNumber<char>(0x01);
            break;
        }
        case LOGIN:
            out.writeNumber<char>(0x00);
            break;
        case PLAY:
            out.writeNumber<char>(0x1A);
            break;
        default:
            return;
    }
    MinecraftServer::get().getLogger().info("Disconnecting player " + playerName + " for reason: " + reason.asPlainText());
    out.WriteString(reason.asString());
    sendPacket(out);
}

void Player::sendKeepAlive() {
    ticksSinceLastKeepAlive = 0;
    lastKeepAlive = rand() & 0xFFFFFFFF;
    Packet out;
    switch (state) {
        case CONFIGURATION:
            out.writeNumber<char>(0x03);
            break;
        default:
            return;
    }
    out.writeNumber<long>(lastKeepAlive);
    sendPacket(out);
}

void Player::handleKeepAlive(Packet& in) {
    long id = in.readNumber<long>();
    if (id != lastKeepAlive)
        kick({"Timed out"});
    ticksSinceLastKeepAlive = 0;
    lastKeepAlive = -1;
}

bool Player::keepConnection() {
    return connected;
}

/*
    Handshake Handler Functions
*/

void Player::handleHandshake(Packet& in) {
    int version = in.ReadVarInt();
    std::string addr = in.ReadString();
    unsigned short port = in.readNumber<unsigned short>();
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
    Packet out;
    out.writeNumber<char>(0x00);
    out.WriteString(MinecraftServer::get().generateMOTD().asString());
    sendPacket(out);
}

void Player::handlePingRequest(Packet& in) {
    Packet out;
    out.writeNumber<char>(0x01);
    out.writeNumber<long>(in.readNumber<long>());
    sendPacket(out);
}

/*
    Login Handle Functions
*/

void Player::handleLoginStart(Packet& in) {
    playerName = in.ReadString();
    playerUUID = UUID(in.readNumber<uint64_t>(), in.readNumber<uint64_t>());
    MinecraftServer::get().getLogger().info("UUID of player " + playerName + " is " + playerUUID.getUUID());
    if (MinecraftServer::get().isOnline()) {
        // send encryption resquest and authorize player
    } else {
        Packet out;
        out.writeNumber<char>(0x02);
        out.writeNumber<uint64_t>(playerUUID.getMostSigBits());
        out.writeNumber<uint64_t>(playerUUID.getLeastSigBits());
        out.WriteString(playerName);
        out.WriteVarInt(0);
        sendPacket(out);
    }
}

void Player::handleEncryptionResponse(Packet& in) {

}

void Player::handlePluginResponse(Packet& in) {

}

void Player::handleLoginAcknowledged(Packet& in) {
    state = CONFIGURATION;
    Packet out;
    out.writeNumber<char>(0x02);
    sendPacket(out);
}

/*
    Configuration Handler Functions
*/

void Player::handleInformation(Packet& in) {

}

void Player::handlePluginMessage(Packet& in) {

}

void Player::handleAcknowledgeConfigFinish(Packet& in) {
    state = PLAY;
    kick({"kill yourself"});
}

void Player::handlePong(Packet& in) { }

void Player::handleResourcePackResponse(Packet& in) {

}