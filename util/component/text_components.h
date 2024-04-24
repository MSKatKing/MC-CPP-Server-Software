#ifndef TEXT_COMPONENTS_H
#define TEXT_COMPONENTS_H

#include "../../io/json.h"
#include <string.h>
#include "../identifiers/indentifier.h"

#define COMPONENT_BLACK "#000000"
#define COMPONENT_DARK_BLUE "#0000aa"
#define COMPONENT_DARK_GREEN "#00aa00"
#define COMPONENT_DARK_CYAN "#00aaaa"
#define COMPONENT_DARK_RED "#aa0000"
#define COMPONENT_PURPLE "#aa00aa"
#define COMPONENT_GOLD "#ffaa00"
#define COMPONENT_GRAY "#aaaaaa"
#define COMPONENT_DARK_GRAY "#555555"
#define COMPONENT_BLUE "#5555ff"
#define COMPONENT_BRIGHT_GREEN "#55ff55"
#define COMPONENT_CYAN "#55ffff"
#define COMPONENT_RED "#ff5555"
#define COMPONENT_PINK "#ff55ff"
#define COMPONENT_YELLOW "#ffff55"
#define COMPONENT_WHITE "#ffffff"

#define COMPONENT_BG_BLACK "#000000"
#define COMPONENT_BG_DARK_BLUE "#00002a"
#define COMPONENT_BG_DARK_GREEN "#002a00"
#define COMPONENT_BG_DARK_CYAN "#002a2a"
#define COMPONENT_BG_DARK_RED "#2a0000"
#define COMPONENT_BG_PURPLE "#2a002a"
#define COMPONENT_BG_GOLD "#2a2a00"
#define COMPONENT_BG_GRAY "#2a2a2a"
#define COMPONENT_BG_DARK_GRAY "#151515"
#define COMPONENT_BG_BLUE "#15153f"
#define COMPONENT_BG_BRIGHT_GREEN "#153f15"
#define COMPONENT_BG_CYAN "#153f3f"
#define COMPONENT_BG_RED "#3f1515"
#define COMPONENT_BG_PINK "#3f153f"
#define COMPONENT_BG_YELLOW "#3f3f15"
#define COMPONENT_BG_WHITE "#3f3f3f"

#define SERVER_LIST_DEFAULT "#848484"

enum ClickEventOption {
    OPEN_URL,
    RUN_COMMAND,
    SUGGEST_COMMAND,
    CHANGE_PAGE,
    COPY_TO_CLIPBOARD
};

class TextComponent {
private:
    std::string plainText;

    std::string color;

    bool bold, italic, underlined, strikethrough, obfuscated;
    
    Identifier font = FONT_DEFAULT;
    JSON clickEvent, hoverEvent;

    std::vector<TextComponent> extras;

public:
    TextComponent(const std::string& text) {
        plainText = text;
    }

    void setBold(bool value);
    void setItalic(bool value);
    void setUnderlined(bool value);
    void setStrikethrough(bool value);
    void setObfuscated(bool value);

    void setColor(const std::string value);

    void setClickEvent(ClickEventOption option, const std::string& value);

    const std::string asString() const;
    const JSON asJSON() const;
    const std::string& asPlainText() const;
};



#endif //TEXT_COMPONENTS_H
