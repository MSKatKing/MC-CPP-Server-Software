#ifndef TEXT_COMPONENTS_H
#define TEXT_COMPONENTS_H

#include "../../io/json.h"
#include <string.h>

class TextComponent {
private:
    JSON json;
    std::string plainText;

public:
    TextComponent(const std::string& text) {
        json.writeString("text", text);
        plainText = text;
    }

    const std::string asString() const;
    const JSON asJSON() const;
    const std::string& asPlainText() const;
};



#endif //TEXT_COMPONENTS_H
