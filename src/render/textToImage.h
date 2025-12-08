#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Image.hpp>
#include <string>

sf::Image textToImage(
    const std::string &text, const sf::Font &font, unsigned int characterSize, sf::Color color);
