#ifndef IDENTIFIER_H
#define IDENTIFIER_H

#include <string>
#include <vector>
#include <sstream>

class Identifier {
private:
    std::string name;
    std::string value;

public:
    Identifier(const std::string& name, const std::string& value): name(name), value(value) {}
    Identifier(const std::string& value): name("minecraft"), value(value) {}

    const std::string asString() const {
        return name + ":" + value;
    }

    std::string& getNamespace() {
        return name;
    }

    std::string& getName() {
        return value;
    }

    static Identifier fromString(const std::string& str) {
        std::vector<std::string> parts;
        std::istringstream iss(str);
        std::string part;

        while (std::getline(iss, part, ':'))
            parts.push_back(part);

        if (parts.size() < 2) return Identifier("error");

        Identifier id(parts[0], parts[1]);

        return id;
    }
};

#define FONT_DEFAULT Identifier("default")
#define FONT_GNU_UNIFORM Identifier("uniform")
#define FONT_GALACTIC Identifier("alt")
#define FONT_ILLAGERALT Identifier("illageralt")

#endif