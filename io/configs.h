#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

//#include "console.h"

namespace Configurations {

    class CFGConfiguration {
    private:
        std::string path;
        std::unordered_map<std::string, std::string> config;

        void parseLine(const std::string& line) {
            std::istringstream iss(line);
            if(std::string key, value; std::getline(iss, key, '=') && std::getline(iss, value)) {
                trim(key);
                trim(value);
                config[key] = value;
            }
        }

        static void trim(std::string& str) {
            if(const size_t start = str.find_first_not_of(" \t"), end = str.find_last_not_of(" \t"); start != std::string::npos && end != std::string::npos) {
                str = str.substr(start, end - start + 1);
            } else {
                str.clear();
            }
        }

    public:
        explicit CFGConfiguration(const std::string& path) : path(path) {
            if(std::ifstream file(path); file) {
                std::string line;
                while(std::getline(file, line)) {
                    parseLine(line);
                }
            }
        }

        std::unordered_map<std::string, std::string>& getMap() {
            return config;
        }

        void set(const std::string& key, const std::string& value) {
            config[key] = value;
        }

        void save() {
            if(std::ofstream file(path); file) {
                for(const auto& pair : config) {
                    file << pair.first << " = " << pair.second << std::endl;
                }
                file.close();
            }
        }

        void reload() {
            if(std::ifstream file(path); file) {
                std::string line;
                while(std::getline(file, line)) {
                    parseLine(line);
                }
            }
        }

        std::string get(const std::string& key, const std::string& def) {
            if(const auto it = config.find(key); it != config.end()) {
                return it->second;
            }
            config[key] = def;
            return def;
        }
    };

}
