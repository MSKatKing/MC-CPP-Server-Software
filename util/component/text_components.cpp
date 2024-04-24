#include "text_components.h"
//
// Created by wait4 on 4/17/2024.
//

#include "text_components.h"

const std::string TextComponent::asString() const {
    return asJSON().asString();
}

const JSON TextComponent::asJSON() const {
    JSON out;
    out.writeString("text", plainText);
    if (!color.empty()) out.writeString("color", color);
    if (bold) out.writeBool("bold", bold);
    if (italic) out.writeBool("italic", italic);
    if (underlined) out.writeBool("underlined", underlined);
    if (strikethrough) out.writeBool("strikethrough", strikethrough);
    if (obfuscated) out.writeBool("obfuscated", obfuscated);
    out.writeString("font", font.asString());
    return out;
}

const std::string& TextComponent::asPlainText() const {
    return plainText;
}

void TextComponent::setBold(bool value) {
    bold = value;
}

void TextComponent::setItalic(bool value) {
    italic = value;
}

void TextComponent::setUnderlined(bool value) {
    underlined = value;
}

void TextComponent::setStrikethrough(bool value) {
    strikethrough = value;
}

void TextComponent::setObfuscated(bool value) {
    obfuscated = value;
}

void TextComponent::setColor(const std::string value) {
    color = value;
}

