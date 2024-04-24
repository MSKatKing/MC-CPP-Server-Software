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

#ifdef WIN32

Player::Player(SOCKET socket) : socket(socket) {
    listenerThread = std::thread(&Player::receivePacket, this);
    //formatSocket();
}
#else
Player::Player(int socket) : socket(socket) {
    listenerThread = std::thread(&Player::receivePacket, this);
    //formatSocket();
}
#endif

Player::~Player() {
    connected = false;
#ifdef WIN32
    closesocket(getSocket());
#else
    close(getSocket());
#endif

    listenerThread.join();
}

void Player::sendPacket(const Packet& in) {
    Packet f = in.Finalize();
    send(getSocket(), f.Sendable(), f.GetSize(), 0);
}

//Packet Player::recievePacket() {
//    char buffer[1024];
//    const int bytesRecieved = recv(getSocket(), buffer, sizeof(buffer), 0);
//    if (bytesRecieved < 0) {
//        return Packet();
//    }
//    if (bytesRecieved == 0) {
//        kick({"Invalid packet data recieved!"});
//        return Packet();
//    }
//
//    Packet in(buffer);
//
//    return in;
//}

void Player::tick() {
    if (!connected) return;

    while (!packetQueue.empty()) {
        Packet p = packetQueue.front();
        packetQueue.pop();

        unsigned char legacy = p.readNumber<unsigned char>();

        if (legacy == 0xFE) {
            handleLegacyPing(p);
            return;
        }

        p.SetCursor(0);

        int length = p.ReadVarInt();
        while(length > 0) {
            int id = p.ReadVarInt();

            MinecraftServer::get().getLogger().info(std::to_string(id));

            try {
                auto& handler = Player::packetHandlers.at(state).at(id);
                (this->*handler)(p);
            } catch (const std::out_of_range& exception) {
                kick({"Invalid player state or packet id!"});
                break;
            }

            length = p.ReadVarInt();
        }
    }
    
    ticksSinceLastKeepAlive++;
    if (lastKeepAlive != 0) {
        if (ticksSinceLastKeepAlive > 300) // TODO: replace constant with config variable
            kick({"Timed out"});
    } else {
        if (ticksSinceLastKeepAlive > 200)
            sendKeepAlive();
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
    if(playerName != "[unknown]") MinecraftServer::get().getLogger().info("Disconnecting player " + playerName + " for reason: " + reason.asPlainText());
    reason.setColor(COMPONENT_RED);
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

bool Player::keepConnection() const {
    return connected;
}

/*
    Handshake Handler Functions
*/

void Player::handleHandshake(Packet& in) {
    int version = in.ReadVarInt();
    std::string addr = in.ReadString();
    unsigned short port = in.readNumber<unsigned short>();
    int s = in.ReadVarInt();

    if (s == 1)
        setState(STATUS);
    else if (s == 2)
        setState(LOGIN);
}

void Player::handleLegacyPing(Packet& in) {
    Packet out;
    out.writeNumber<char>(0xFF);
    std::string str = "§1\0" + 
        std::to_string(VERSION_ID) + "\0" + 
        VERSION_NAME + "\0" + 
        MinecraftServer::get().getMOTDMessage() + "\0" +
        std::to_string(MinecraftServer::get().getOnlinePlayers().size()) + "\0" + 
        std::to_string(MinecraftServer::get().getMaxPlayers());
    out.writeNumber<short>(str.size());
    out.writeArray<char>(str.c_str(), str.size());
    sendPacket(out);
    kick({""});
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
    out.writeNumber<int64_t>(in.readNumber<int64_t>());
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
        Packet out;
        out.writeNumber<char>(0x01);
        out.WriteString("");
        KeyPair keys = MinecraftServer::get().getKeys();
        MinecraftServer::get().getLogger().info(std::to_string(keys.publicLen));
        out.WriteVarInt(keys.publicLen);
        out.writeArray<unsigned char>(keys.publicKey, keys.publicLen);
        token.resize(4);
        out.WriteVarInt(token.size());
        for (int i = 0; i < 4; i++) {
            char tokenByte = static_cast<char>(rand() & 0xFF);
            token[i] = tokenByte;
            out.writeNumber<char>(tokenByte);
        }
        sendPacket(out);
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
    MinecraftServer::get().getLogger().info("response");
}

void Player::handlePluginResponse(Packet& in) {

}

void Player::handleLoginAcknowledged(Packet& in) {
    state = CONFIGURATION;
    kick({"Not supported! Please tell server admins to check if there's any new updates to the server software!"});
}

/*
    Configuration Handler Functions
*/

void Player::handleInformation(Packet& in) {

}

void Player::handlePluginMessage(Packet& in) {
    Identifier id = Identifier::fromString(in.ReadString());
    if (id.getName() == "brand") brand = in.ReadString();
}

void Player::handleAcknowledgeConfigFinish(Packet& in) {
    state = PLAY;
    kick({"kill yourself"});
}

void Player::handlePong(Packet& in) { }

void Player::handleResourcePackResponse(Packet& in) {

}