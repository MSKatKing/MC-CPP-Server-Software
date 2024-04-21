#ifndef MINECRAFT_SERVER_H
#define MINECRAFT_SERVER_H
#include <condition_variable>
#include <thread>
#include <mutex>
#include <vector>
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

#define VERSION_NAME "1.20.4"
#define VERSION_ID 765

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

    MinecraftServer(const MinecraftServer&) = delete;

private:
    MinecraftServer();

    void listenForClients();
    void inputThreadFunction();

    void tick();

    std::string checkConsoleCommand();
    void loadConfig();

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

    std::vector<Player> players;

    long long publicKey;
    long long privateKey;
    long long modulus;

    static MinecraftServer instance;

    // Settings
    Configurations::CFGConfiguration config;

    short port;
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
