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
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
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

int Player::encrypt(const char* input, size_t length, unsigned char* output) {
    AES_KEY aesKey;
    if (AES_set_encrypt_key(reinterpret_cast<const unsigned char*>(sharedSecret.data()), sharedSecret.size() * 8, &aesKey) != 0) {
        return -1;
    }

    AES_cfb128_encrypt(reinterpret_cast<const unsigned char*>(input), output, length, &aesKey, reinterpret_cast<unsigned char*>(const_cast<char*>(sharedSecret.data())), nullptr, AES_ENCRYPT);

    return length;
}

std::string Player::calculateMinecraftSHA1() {
    // Initialize SHA1 context
    SHA_CTX sha1;
    SHA1_Init(&sha1);
    std::string serverId;

    // Update context with ASCII encoding of server id
    SHA1_Update(&sha1, serverId.c_str(), serverId.length());

    // Update context with shared secret
    SHA1_Update(&sha1, sharedSecret.c_str(), sharedSecret.length());

    KeyPair keys = MinecraftServer::get().getKeys();

    // Update context with server's encoded public key
    SHA1_Update(&sha1, keys.publicKey, keys.publicLen);

    // Finalize SHA1 hash
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1_Final(digest, &sha1);

    // Convert digest to hexadecimal string
    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }

    std::string hash = ss.str();

    // Handle Minecraft's non-standard hexadecimal representation
    bool negative = (digest[0] & 0x80) != 0;
    if (negative) {
        hash.insert(0, "-");
    }

    return hash;
}

#ifdef WIN32
Player::Player(SOCKET socket) : playerSocket(socket), encryptionEnabled(false) {
    listenerThread = std::thread(&Player::receivePacket, this);
    //formatSocket();
}
#else
Player::Player(int socket) : playerSocket(socket), encryptionEnabled(false) {
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
    Packet f;
    if (encryptionEnabled) {
        unsigned char data[in.GetSize()];
        int size = encrypt(in.buffer, in.GetSize(), data);
        if (size == -1) {
            kick({"Error encrypting data!"});
            return;
        }
        f.WriteVarInt(size);
        f.WriteVarInt(in.id);
        f.writeArray<unsigned char>(data, size);
    } else f = in.Finalize();
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
    out.setID(0xFF);
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
    out.setID(0x00);
    out.WriteString(MinecraftServer::get().generateMOTD().asString());
    sendPacket(out);
}

void Player::handlePingRequest(Packet& in) {
    Packet out;
    out.setID(0x01);
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
        out.setID(0x01);
        out.WriteString("");
        KeyPair keys = MinecraftServer::get().getKeys();
        MinecraftServer::get().getLogger().info(std::to_string(keys.publicLen));
        out.WriteVarInt(keys.publicLen);
        out.writeArray<unsigned char>(keys.publicKey, keys.publicLen);
        token.resize(4);
        out.WriteVarInt(token.size());
        for (int i = 0; i < 4; i++) {
            unsigned char tokenByte = static_cast<unsigned char>(rand() & 0xFF);
            token[i] = tokenByte;
            out.writeNumber<unsigned char>(tokenByte);
        }
        sendPacket(out);
    } else {
        Packet out;
        out.setID(0x02);
        out.writeNumber<uint64_t>(playerUUID.getMostSigBits());
        out.writeNumber<uint64_t>(playerUUID.getLeastSigBits());
        out.WriteString(playerName);
        out.WriteVarInt(0);
        sendPacket(out);
    }
}

void Player::handleEncryptionResponse(Packet& in) {
    MinecraftServer::get().getLogger().info("response");
    int sharedSecretLength = in.ReadVarInt();
    std::vector<unsigned char> data = in.readArray<unsigned char>(sharedSecretLength);
    Data decrypt = MinecraftServer::get().decrypt(data.data(), sharedSecretLength);
    unsigned char* sharedSecretBytes = decrypt.data;
    sharedSecret.resize(sharedSecretLength);
    std::memcpy(sharedSecret.data(), sharedSecretBytes, sharedSecretLength);
    int tokenLength = in.ReadVarInt();
    std::vector<unsigned char> readToken = in.readArray<unsigned char>(tokenLength);
    Data decryptToken = MinecraftServer::get().decrypt(readToken.data(), tokenLength);
    for (int i = 0; i < token.size(); i++) {
        if (token[i] != decryptToken.data[i]) {
            kick({"Validation token is invalid!"});
            return;
        }
    }

    /*
        need to send shit to mojang to confirm that the player can connect, also need to collect player properties from mojang servers
        look more into it w/ chatgpt and wiki.vg, for now assume that everything is ok and immediately send login finish
    */
    std::cout << getPlayerDataFromMojang().asString() << std::endl;

    if (MinecraftServer::get().getCompressionAmount() > 0) {
        Packet out;
        out.setID(0x03);
        out.WriteVarInt(MinecraftServer::get().getCompressionAmount());
        sendPacket(out);
    }

    Packet out;
    out.setID(0x02);
    out.writeNumber<uint64_t>(playerUUID.getMostSigBits());
    out.writeNumber<uint64_t>(playerUUID.getLeastSigBits());
    out.WriteString(playerName);
    out.WriteVarInt(0);
    sendPacket(out);

    encryptionEnabled = true;
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