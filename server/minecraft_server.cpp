#include "minecraft_server.h"
//
// Created by wait4 on 4/7/2024.
//

#include "minecraft_server.h"

#include <iostream>
#include <thread>
#include <valarray>
#include <algorithm>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include "../io/json.h"
#include "../networking/packets/packet.h"

bool stob(const std::string& str) {
    std::string lowercaseStr = str;
    std::transform(lowercaseStr.begin(), lowercaseStr.end(), lowercaseStr.begin(), ::tolower);

    if (lowercaseStr == "true" || lowercaseStr == "1") {
        return true;
    } else {
        return false;
    }
}

void MinecraftServer::loadConfig() {
    config.reload();

    port = std::stoi(config.get("port", "25565"));
    localhost = stob(config.get("localhost", "false"));
    onlineMode = stob(config.get("online-mode", "true"));
    maxPlayers = std::stoi(config.get("max-players", "20"));
    motd = config.get("motd", "A C++ Minecraft Server");
    enforcesSecureChat = stob(config.get("enforces-secure-chat", "true"));
    previewsChat = stob(config.get("previews-chat", "true"));
    compressionAmount = std::stoi(config.get("compression-amount", "-1"));

    config.save();
}

MinecraftServer::MinecraftServer() : running(true), ticks(0), logger("THREAD-MAIN"), config("server-config.cfg") {
    logger.info("Intitializing server...");

    logger.info("Loading libraries...");

    EVP_PKEY* pKey = EVP_PKEY_new();
    RSA* rsa = RSA_generate_key(1024, RSA_F4, NULL, NULL);
    EVP_PKEY_assign_RSA(pKey, rsa);

    RSA* pubRSA = EVP_PKEY_get1_RSA(pKey);
    unsigned char* publicKeyDER = NULL;
    int publicKeyDERLength = i2d_RSA_PUBKEY(pubRSA, &publicKeyDER);

    RSA* privRSA = EVP_PKEY_get1_RSA(pKey);
    unsigned char* privateKeyDER = NULL;
    int privateKeyDERLength = i2d_RSAPrivateKey(privRSA, &privateKeyDER);

    keys.publicKey = new unsigned char[publicKeyDERLength];
    std::copy(publicKeyDER, publicKeyDER + publicKeyDERLength, keys.publicKey);
    keys.publicLen = publicKeyDERLength;

    keys.privateKey = new unsigned char[privateKeyDERLength];
    std::copy(privateKeyDER, privateKeyDER + privateKeyDERLength, keys.privateKey);
    keys.privateLen = privateKeyDERLength;

    for (int i = 0; i < keys.publicLen; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(keys.publicKey[i]) << " ";
    }
    std::cout << "\n\n" << keys.publicLen << std::endl;

    EVP_PKEY_free(pKey);
    RSA_free(rsa);
    OPENSSL_free(publicKeyDER);
    OPENSSL_free(privateKeyDER);

    loadConfig();

    commands.push_back(new Commands::StopCommand());
    commands.push_back(new Commands::ReloadCommand());
    commands.push_back(new Commands::HelpCommand());
    commands.push_back(new Commands::KickCommand());
    
#ifdef WIN32
    WSAData wsa;
    int wsOk = WSAStartup(0x0202, &wsa);
    if (wsOk != 0) {
        logger.error("Couldn't initialize Winsock! Cannot continue, quitting.");
        running = false;
        return;
    }
#endif

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(serverSocket < 0) {
        logger.error("Can't create socket! Cannot continue, quitting.");
        running = false;
#ifdef WIN32
        WSACleanup();
#endif
        return;
    }

    sockaddr_in hint{};
    memset(&hint, 0, sizeof(hint));
    hint.sin_family = AF_INET;
    hint.sin_port = htons(port);
    hint.sin_addr.s_addr = INADDR_ANY;

    if(bind(serverSocket, reinterpret_cast<const sockaddr*>(&hint), sizeof(hint)) != 0) {
        logger.error("Can't bind to port! Is the port open? Cannot continue, quitting.");
        running = false;
#ifdef WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif
        return;
    }

    if(listen(serverSocket, SOMAXCONN) != 0) {
        logger.error("Can't listen on socket! Cannot continue, quitting.");
        running = false;
#ifdef WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif
        return;
    }

    logger.info("Successfully binded! Server listening on port " + std::to_string(port));
}

void MinecraftServer::listenForClients() {
    while(running) {

        sockaddr_in clientAddr;
        memset(&clientAddr, 0, sizeof(clientAddr));
        socklen_t clientAddrLen = sizeof(clientAddr);

#ifdef WIN32
        SOCKET clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
#else
        int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
#endif

#ifdef WIN32
        if(clientSocket == INVALID_SOCKET && running) {
#else
        if(clientSocket == -1 && running) {
#endif
            logger.error("Unable to accept incoming connection.");
            continue;
        }

        Player* player = new Player(clientSocket);

        players.push_back(player);
    }
}

void MinecraftServer::start() {
    if(!running) return;

    logger.info("Server starting...");

    listenerThread = std::thread(&MinecraftServer::listenForClients, this);

    consoleThread = std::thread(&MinecraftServer::inputThreadFunction, this);

    constexpr std::chrono::milliseconds targetTickDuration(50);

    logger.info("Timings started!");

    logger.info("Server started! Type ? for command help.");

    while(running) {
        const auto tickStartTime = std::chrono::steady_clock::now();

        tick();

        const auto tickEndTime = std::chrono::steady_clock::now();
        const auto tickDuration = std::chrono::duration_cast<std::chrono::milliseconds>(tickEndTime - tickStartTime);

        if(const auto remainingTime = targetTickDuration - tickDuration; remainingTime > std::chrono::milliseconds(0))
            std::this_thread::sleep_for(remainingTime);
    }
}

void MinecraftServer::stop() {
    if(running) {
        logger.info("Server stopping...");

        running = false;

        consoleThread.detach();

        listenerThread.detach();

        for(Player* p : players) p->kick({"Server stopping."});
        players.clear();

        if (keys.publicKey) OPENSSL_free((void*) keys.publicKey);
        if (keys.privateKey) OPENSSL_free((void*) keys.privateKey);

#ifdef WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif

        logger.info("Server stopped!");
        logger.close();
    }
}

void MinecraftServer::tick() {
    // Gather any console input
    if(const std::string consoleInput = checkConsoleCommand(); !consoleInput.empty()) {
        size_t space = consoleInput.find(' ');

        std::string name;
        std::vector<std::string> args;

        if (space != std::string::npos) {
            name = consoleInput.substr(0, consoleInput.find(' '));
            std::istringstream iss(consoleInput.substr(consoleInput.find(' ') + 1));
            std::string arg;
            while (iss >> arg)
                args.push_back(arg);
        } else {
            name = consoleInput;
        }
        bool executed = false;
        for (Commands::Command* c : commands) {
            if (c->compare(name)) {
                if (!c->execute(args)) {
                    logger.error("Error occured while executing command '" + name + "'!");
                }
                executed = true;
                break;
            }
        }
        if(!executed) logger.info("Unknown command '" + consoleInput + "'! Type 'help' for a list of commands.");
        command.clear();
    }

    players.erase(
        std::remove_if(players.begin(), players.end(), [](const Player* p) {
            if (!p->keepConnection()) delete p;
            return !p->keepConnection();
        }), players.end()
    );
    for (Player* p : players)
        p->tick();
}

std::string MinecraftServer::checkConsoleCommand() {
    std::unique_lock lock(consoleMutex);
    consoleCV.wait_for(lock, std::chrono::milliseconds(10), [this] { return !command.empty(); });

    return command;
}


void MinecraftServer::inputThreadFunction() {
    while(running) {
        std::string input;
        std::getline(std::cin, input);

        std::unique_lock lock(consoleMutex);

        command = input;

        consoleCV.notify_one();
    }
}

const Logger& MinecraftServer::getLogger() const {
    return logger;
}

JSON MinecraftServer::generateMOTD() const {
    JSON output;
    JSON version;
    version.writeString("name", VERSION_NAME);
    version.writeInt("protocol", VERSION_ID);
    output.writeJson("version", version);
    JSON players;
    players.writeInt("max", maxPlayers);
    players.writeInt("online", 0);
    output.writeJson("players", players);
    TextComponent desc(motd);
    output.writeJson("description", desc.asJSON());
    output.writeBool("enforcesSecureChat", enforcesSecureChat);
    output.writeBool("previewsChat", previewsChat);
    return output;
}

bool MinecraftServer::isOnline() const {
    return onlineMode;
}
