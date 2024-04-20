#include "text_components.h"
//
// Created by wait4 on 4/17/2024.
//

#include "text_components.h"

const std::string TextComponent::asString() const {
    return json.asString();
}

const JSON TextComponent::asJSON() const {
    return json;
}

const std::string& TextComponent::asPlainText() const {
    return plainText;
}
