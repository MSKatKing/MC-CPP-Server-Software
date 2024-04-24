//
// Created by wait4 on 4/7/2024.
//

#ifndef JSON_H
#define JSON_H
#include <string>
#include <unordered_map>
#include <vector>


class JSON {
public:
    JSON() {}

    void writeInt(const std::string& key, int value) {
        data[key] = std::to_string(value);
    }

    void writeBool(const std::string& key, bool value) {
        data[key] = value ? "true" : "false";
    }

    void writeString(const std::string& key, const std::string& value) {
        data[key] = "\"" + value + "\"";
    }

    void writeJson(const std::string& key, const JSON& json) {
        data[key] = json.asString();
    }

    void writeJsonList(const std::string& key, const std::vector<JSON>& d) {
        std::string value = "[";
        for (JSON j : d) value += j.asString() + ",";
        value.pop_back();
        value += "]";
        data[key] = value;
    }

    const std::string asString() const {
        std::string result = "{";
        for(auto it = data.begin(); it != data.end(); it++) {
            if(it != data.begin()) result += ",";
            result += "\"" + it->first + "\":" + it->second;
        }
        result += "}";
        return result;
    }

private:
    std::unordered_map<std::string, std::string> data;
};



#endif //JSON_H
