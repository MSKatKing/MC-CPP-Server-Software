#include "minecraft_server.h"
//
// Created by wait4 on 4/7/2024.
//

#include "minecraft_server.h"

#include <iostream>
#include <thread>
#include <valarray>
#include <algorithm>

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

    loadConfig();

#ifdef WIN32
    WSAData wsData{};
    WORD ver = MAKEWORD(2, 2);
    int wsOk = WSAStartup(ver, &wsData);
    if(wsOk != 0) {
        logger.error("Couldn't initialize Winsock! Cannot continue, quitting.");
        running = false;
        return;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(serverSocket == INVALID_SOCKET) {
        logger.error("Can't create socket! Cannot continue, quitting.");
        running = false;
        WSACleanup();
        return;
    }

    sockaddr_in hint{};
    hint.sin_family = AF_INET;
    hint.sin_port = htons(port);
    hint.sin_addr.s_addr = INADDR_ANY;

    if(bind(serverSocket, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
        logger.error("Can't bind to port! Is the port open? Cannot continue, quitting.");
        running = false;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    if(listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        logger.error("Can't listen on socket! Cannot continue, quitting.");
        running = false;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }
#else
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == -1) {
        logger.error("Can't create socket! Cannot continue, quitting.");
        running = false;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if(bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        logger.error("Can't bind to port! Is the port open? Cannot continue, quitting.");
        running = false;
        close(serverSocket);
        return;
    }

    if(listen(serverSocket, SOMAXCONN) == -1) {
        logger.error("Can't listen on socket! Cannot continue, quitting.");
        running = false;
        close(serverSocket);
        return;
    }
#endif

    logger.info("Successfully binded! Server listening on port " + config.get("port", "25565"));
}

void MinecraftServer::listenForClients() {
    while(running) {
#if WIN32
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if(clientSocket == INVALID_SOCKET && running) {
            logger.error("Unable to accept incoming connection.");
            continue;
        }
#else
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if(clientSocket == -1 && running) {
            logger.error("Unable to accept incoming connection.");
            continue;
        }
#endif

        Player player(clientSocket);

        logger.info("Client connected!");

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

        for(Player p : players) p.kick({"Server stopping."});

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
        if(consoleInput == "stop" || consoleInput == "s" || consoleInput == "quit") stop();
        else if(consoleInput == "reload") {
            logger.info("Reloading the config...");
            loadConfig();
            logger.info("Successfully reloaded the config!");
        } else if(consoleInput == "help" || consoleInput == "?") {
            logger.info("Command list:");
            logger.info("stop:");
            logger.info("   aliases: s, quit");
            logger.info("   description: Stops the server");
            logger.info("");
            logger.info("help:");
            logger.info("   aliases: ?");
            logger.info("   description: shows this list");
            logger.info("");
            logger.info("reload:");
            logger.info("   aliases: ?");
            logger.info("   description: Reloads the server config");
        }
        else logger.info("Unknown command '" + consoleInput + "'! Type 'help' for a list of commands.");
        command.clear();
    }

    players.erase(
        std::remove_if(players.begin(), players.end(), [](Player& p) {
            return p.keepConnection();
        }), players.end()
    );
    for (Player& p : players)
        p.tick();
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
