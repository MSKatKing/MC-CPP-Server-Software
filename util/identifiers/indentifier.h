#ifndef IDENTIFIER_H
#define IDENTIFIER_H

#include <string>

class Identifier {
private:
    std::string name;
    std::string value;

public:
    Identifier(const std::string& name, const std::string& value): name(name), value(value) {}
    Identifier(const std::string& value): name("minecraft"), value(value) {}

    const std::string asString() {
        return name + ":" + value;
    }
};

#endif