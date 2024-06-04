#ifndef UUID_H
#define UUID_H

#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdint>

class UUID {
private:
    std::string uuid;
    int version;

    bool valid;
    uint64_t mostSig;
    uint64_t leastSig;

    uint64_t htonll(uint64_t val) {
        return ((uint64_t)htonl(val & 0xFFFFFFFF) << 32) | htonl(val >> 32);
    }

    uint32_t htonl(uint32_t val) {
        return ((val & 0xFF000000) >> 24) |
               ((val & 0x00FF0000) >> 8) |
               ((val & 0x0000FF00) << 8) |
               ((val & 0x000000FF) << 24);
    }

public:
    UUID(const std::string& id) {
        std::regex pattern("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
        
        if(!std::regex_match(id, pattern)) {
            valid = false;
            return;
        }

        uuid = id;
        version = std::stoi(std::to_string(uuid[14]));
    }

    UUID(uint64_t mostSigBits, uint64_t leastSigBits) {
        mostSig = mostSigBits;
        leastSig = leastSigBits;

        mostSigBits = htonll(mostSigBits);
        leastSigBits = htonll(leastSigBits);

        std::ostringstream uuidStream;
        // uuidStream << std::hex << std::setw(4) << std::setfill('0') << ((leastSigBits >> 48) & 0xFFFF);
        // uuidStream << std::hex << std::setw(12) << std::setfill('0') << (leastSigBits & 0xFFFFFFFFFFFF);
        // uuidStream << std::hex << std::setw(8) << std::setfill('0') << ((mostSigBits >> 32) & 0xFFFFFFFF);
        // uuidStream << std::hex << std::setw(4) << std::setfill('0') << ((mostSigBits >> 16) & 0xFFFF);
        // uuidStream << std::hex << std::setw(4) << std::setfill('0') << (mostSigBits & 0xFFFF);

        uuidStream << std::hex << std::setw(4) << std::setfill('0') << ((leastSigBits >> 48) & 0xFFFF);
    uuidStream << std::hex << std::setw(4) << std::setfill('0') << ((leastSigBits >> 32) & 0xFFFF);
    uuidStream << "-";
    uuidStream << std::hex << std::setw(4) << std::setfill('0') << ((leastSigBits >> 16) & 0xFFFF);
    uuidStream << "-";
    uuidStream << std::hex << std::setw(4) << std::setfill('0') << (leastSigBits & 0xFFFF);
    uuidStream << "-";
    uuidStream << std::hex << std::setw(4) << std::setfill('0') << ((mostSigBits >> 48) & 0xFFFF);
    uuidStream << "-";
    uuidStream << std::hex << std::setw(4) << std::setfill('0') << ((mostSigBits >> 32) & 0xFFFF);
    uuidStream << std::hex << std::setw(4) << std::setfill('0') << ((mostSigBits >> 16) & 0xFFFF);
    uuidStream << std::hex << std::setw(4) << std::setfill('0') << (mostSigBits & 0xFFFF);


        uuid = uuidStream.str();
        // uuid = uuid.substr(0, 8) + "-" +
        //     uuid.substr(8, 4) + "-" +
        //     uuid.substr(12, 4) + "-" +
        //     uuid.substr(16, 4) + "-" +
        //     uuid.substr(20);
        version = std::stoi(std::to_string(uuid[14]));
    }

    int getVersion() const {
        return version;
    }

    std::string getUUID() const {
        return uuid;
    }

    uint64_t getMostSigBits() const {
        return mostSig;
    }

    uint64_t getLeastSigBits() const {
        return leastSig;
    }
};

#define INVALID_UUID UUID("00000000-0000-0000-0000-000000000000")

#endif