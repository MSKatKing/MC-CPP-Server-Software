#ifndef MINECRAFT_SERVER_H
#define MINECRAFT_SERVER_H
#include <condition_variable>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#ifdef WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include "client.h"
#include "../io/logger.h"
#include "../io/configs.h"
#include "../minecraft/chat/commands.h"

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/asn1.h>
#include <openssl/evp.h>

#define VERSION_NAME "1.20.4"
#define VERSION_ID 765

struct KeyPair {
    unsigned char* publicKey;
    int publicLen;

    unsigned char* privateKey;
    int privateLen;
};

class MinecraftServer {
public:
    void start();
    void stop();

    const Logger& getLogger() const;
    JSON generateMOTD() const;
    bool isOnline() const;

    static MinecraftServer& get() {
        static MinecraftServer instance;
        return instance;
    }

    void loadConfig();
    Configurations::CFGConfiguration& getConfig() {
        return config;
    }

    std::vector<Commands::Command*>& getCommandList() {
        return commands;
    }
    
    Player* getPlayerByName(const std::string& name) {
        for (Player* p : players) {
            if (compareIgnoreCase(p->getName(), name)) return p;
        }
        return nullptr;
    }

    std::vector<Player*>& getOnlinePlayers() {
        return players;
    }

    std::string& getMOTDMessage() {
        return motd;
    }

    int getMaxPlayers() {
        return maxPlayers;
    }

    MinecraftServer(const MinecraftServer&) = delete;

    KeyPair getKeys() {
        return keys;
    }

private:
    MinecraftServer();

    void listenForClients();
    void inputThreadFunction();

    void tick();

    std::string checkConsoleCommand();

    KeyPair keys;

    bool compareIgnoreCase(const std::string& str1, const std::string& str2) {
        std::string upperStr1 = str1;
        std::string upperStr2 = str2;
        std::transform(upperStr1.begin(), upperStr1.end(), upperStr1.begin(), ::toupper);
        std::transform(upperStr2.begin(), upperStr2.end(), upperStr2.begin(), ::toupper);

        return upperStr1 == upperStr2;
    }

    std::mutex mutex;
    std::condition_variable cv;
    std::thread listenerThread;
    std::thread consoleThread;
    bool running;
    int ticks;
    Logger logger;

    std::mutex consoleMutex;
    std::condition_variable consoleCV;
    std::string command;

    std::vector<Player*> players;
    std::vector<Commands::Command*> commands;

    long long publicKey;
    long long privateKey;
    long long modulus;

    static MinecraftServer instance;

    // Settings
    Configurations::CFGConfiguration config;

    short port;
    bool localhost;
    int maxPlayers;
    bool onlineMode;
    bool enforcesSecureChat;
    bool previewsChat;
    int compressionAmount;

    std::string motd;

#ifdef WIN32
    SOCKET serverSocket;
#else
    int serverSocket;
#endif
};


#endif //MINECRAFT_SERVER_H
