#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <utility>
#include <functional>
#include <unordered_map>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include "../networking/packets/packet.h"
#include "../util/component/text_components.h"
#include "../util/identifiers/uuid.h"
#include "../util/identifiers/indentifier.h"
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

class Player;

using HandlerPointer = void(Player::*)(Packet&);

class MinecraftServer;

enum ClientState {
    HANDSHAKE,
    STATUS,
    LOGIN,
    CONFIGURATION,
    PLAY
};

#ifdef WIN32
#define close closesocket
#else
#define SOCKET int
#endif

class Player {
private:
#ifdef WIN32
    SOCKET playerSocket;
#else
    int playerSocket;
#endif

    std::queue<Packet> packetQueue;

    std::string playerName = "[unknown]";
    UUID playerUUID = INVALID_UUID;

    std::string brand = "unknown";

    ClientState state = HANDSHAKE;
    bool connected = true;

    std::vector<unsigned char> token;

    long lastKeepAlive = -1;
    int ticksSinceLastKeepAlive = 0;

    void sendPacket(const Packet& final);
    //Packet recievePacket();

    void formatSocket();

    void receivePacket() {
        while (connected) {
            char buffer[65536] = {0};
            const int bytesRecieved = static_cast<int>(recv(playerSocket, buffer, sizeof(buffer), 0));
            if (bytesRecieved <= 0) {
                connected = false;
                break;
            }

            Packet p(buffer);

            if (state == HANDSHAKE || state == STATUS || state == LOGIN) {
                unsigned char legacy = p.readNumber<unsigned char>();

                if (legacy == 0xFE) {
                    handleLegacyPing(p);
                    break;
                }

                p.SetCursor(0);

                int length = p.ReadVarInt();
                while (length > 0) {
                    int id = p.ReadVarInt();

                    try {
                        auto& handler = Player::packetHandlers.at(state).at(id);
                        (this->*handler)(p);
                    } catch (const std::out_of_range& exception) {
                        kick({"Invalid player state or packet id!"});
                        std::cout << state << std::endl;
                        std::cout << id << std::endl;
                        break;
                    }

                    length = p.ReadVarInt();
                }
                continue;
            }

            {
                packetQueue.push(p);
            }
        }
    }

    std::thread listenerThread;

    bool encryptionEnabled;
    std::string sharedSecret;

    int encrypt(const char* input, size_t length, unsigned char* output);
    int decrypt(const char* input, size_t length, unsigned char* output);

    JSON getPlayerDataFromMojang() {
        std::string request = "GET /session/minecraft/hasJoined?username=" + playerName + "&serverId=" + calculateMinecraftSHA1() + " HTTP/1.1\r\n"
            "Host: sessionserver.mojang.com\r\n"
            "Connection: close\r\n\r\n";

        struct addrinfo hints, * res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        int status = getaddrinfo("sessionserver.mojang.com", "80", &hints, &res);
        if (status != 0) return {};

#ifdef WIN32
        SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
#else
        int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
#endif
        if (s < 0) return {};

        if (connect(s, res->ai_addr, res->ai_addrlen) < 0) return {};
        if (send(s, request.c_str(), request.size(), 0) < 0) {
            close(s);
            return {};
        }

        char response_buffer[4096];
        size_t bytes_received;
        std::string response_data;

        while ((bytes_received = recv(s, response_buffer, sizeof(response_buffer), 0)) > 0) {
            response_data.append(response_buffer, bytes_received);
        }

        if (bytes_received < 0) {
            close(s);
            return {};
        }

        close(playerSocket);
        freeaddrinfo(res);

        return {response_data};
    }

    std::string calculateMinecraftSHA1();


public:
#ifdef WIN32
    explicit Player(SOCKET socket);
#else
    explicit Player(int socket);
#endif

    ~Player();

#ifdef WIN32
    SOCKET getSocket() const {
        return playerSocket;
    }
#else
    int getSocket() const {
        return playerSocket;
    }
#endif

    std::string getName() const {
        return playerName;
    }

    std::string getUUID() const {
        return playerUUID.getUUID();
    }

    void setName(std::string newName) {
        playerName = std::move(newName);
    }

    ClientState getState() const {
        return state;
    }

    void setState(ClientState newState) {
        state = newState;
    }

    bool keepConnection() const;

    void tick();

    // Persistant packets
    void kick(TextComponent reason);
    void pluginMessage(Identifier id, const char* data);
    void handlePluginMessage(Packet& in);
    void sendKeepAlive();
    void handleKeepAlive(Packet& in);
    void sendPingResponse(long payload);
    void handlePingRequest(Packet& in);
    void handlePong(Packet& in);

    // Handshake packets
    void handleHandshake(Packet& in);
    void handleLegacyPing(Packet& in);

    // Status packets
    void sendMOTD(const JSON& motd);

    void handleStatusRequest(Packet& in);

    // Login packets
    void sendEncryptionRequest();
    void sendLoginSuccess();
    void sendSetCompression();
    void sendPluginRequest();
    
    void handleLoginStart(Packet& in);
    void handleEncryptionResponse(Packet& in);
    void handlePluginResponse(Packet& in);
    void handleLoginAcknowledged(Packet& in);

    // Configuration packets
    void sendFinishConfiguration();
    void sendRegistryData();
    void sendRemoveResourcePack();
    void sendAddResourcePack();
    void sendFeatureFlags();
    void sendUpdateTags();
    
    void handleInformation(Packet& in);
    void handleAcknowledgeConfigFinish(Packet& in);
    void handleResourcePackResponse(Packet& in);

    // Play packets
    void sendDelimiter();
    
    // Entity packets
    void sendSpawnEntity();
    void sendSpawnExpOrb();
    void sendEntityAnimation();
    
    // Block packets
    void sendAcknowledgeBlockChange();
    void sendSetBlockDestroyStage();
    void sendBlockEntityData();
    void sendBlockAction();
    void sendBlockUpdate();
    

    // Misc
    void sendAwardStatistic();
    void sendBossBar();
    void sendChangeDifficulty();
    void sendChunkBatchFinished();
    void sendChunkBatchStart();
    void sendChunkBiomes();
    void sendClearTitles();
    void sendCommandSuggestions();
    void sendCommands();
    void sendCloseContainer();
    void sendSetContainerContent();
    void sendSetContainerProperty();
    void sendSetContainerSlot();
    void sendSetCooldown();
    void sendChatSuggestions();
    void sendDamageEvent();
    void sendDeleteMessage();
    void sendDisguisedChatMessage();
    void sendEntityEvent();
    void sendExplosion();
    void sendUnloadChunk();
    void sendGameEvent();
    void sendOpenHorseScreen();
    void sendHurtAnimation();
    void sendInitializeWorldBorder();
    void sendChunkAndLightData();
    void sendWorldEvent();
    void sendParticle();
    void sendUpdateLight();
    void sendLogin();
    void sendMapData();
    void sendMerchantOffers();
    void sendUpdateEntityPosition();
    void sendUpdateEntityPositionAndRotation();
    void sendUpdateEntityRotation();
    void sendMoveVehicle();
    void sendOpenBook();
    void sendOpenScreen();
    void sendOpenSignEditor();
    void sendPlaceGhostRecipe();
    void sendPlayerAbilities();
    void sendPlayerChatMessage();
    void sendEndCombat(); //Depricated
    void sendEnterCombat(); //Depicated
    void sendCombatDeath();
    void sendPlayerInfoRemove();
    void sendPlayerInfoUpdate();
    void sendLookAt();
    void sendSynchronizePlayerPosition();
    void sendUpdateRecipeBook();
    void sendRemoveEntities();
    void sendRemoveEntityEffect();
    void sendResetScore();
    void sendRespawn();
    void sendSetHeadRotation();
    void sendUpdateSectionBlocks();
    void sendSelectAdvancementsTab();
    void sendServerData();
    void sendSetActionBarText();
    void sendSetBorderCenter();
    void sendSetBorderLerpSize();
    void sendSetBorderSize();
    void sendSetBorderWarningDelay();
    void sendSetBorderWarningDistance();
    void sendSetCamera();
    void sendSetHeldItem();
    void sendCenterChunk();
    void sendSetRenderDistance();
    void sendSetDefaultSpawnPosition();
    void sendDisplayObjective();
    void sendSetEntityMetadata();
    void sendLinkEntities();
    void sendSetEntityVelocity();
    void sendSetEquipment();
    void sendSetExperience();
    void sendSetHealth();
    void sendUpdateObjectives();
    void sendSetPassengers();
    void sendUpdateTeams();
    void sendUpdateScore();
    void sendSetSimulationDistance();
    void sendSetSubtitleText();
    void sendUpdateTime();
    void sendSetTitleText();
    void sendSetTitleAnimationTimes();
    void sendEntitySoundEffect();
    void sendSoundEffect();
    void sendStartConfiguration();
    void sendStopSound();
    void sendSystemChatMessage();
    void sendSetTabListHeaderAndFooter();
    void sendTagQueryResponse();
    void sendPickupItem();
    void sendTeleportEntity();
    void sendSetTickingState();
    void sendStepTick();
    void sendUpdateAdvancements();
    void sendUpdateAttributes();
    void sendEntityEffect();
    void sendUpdateRecipes();
    //void sendUpdateTags();

    static const std::unordered_map<ClientState, std::unordered_map<char, HandlerPointer>> packetHandlers;
};



#endif //CLIENT_H
