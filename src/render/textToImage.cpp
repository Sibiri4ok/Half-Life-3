#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Text.hpp>
#include <cmath>
#include <string>

sf::Image textToImage(
    const std::string &text, const sf::Font &font, unsigned int characterSize, sf::Color color) {
  sf::Text sfText(font, text, characterSize);
  sfText.setFillColor(color);

  sf::FloatRect bounds = sfText.getLocalBounds();
  unsigned int width = static_cast<unsigned int>(std::ceil(bounds.size.x));
  unsigned int height = static_cast<unsigned int>(std::ceil(bounds.size.y));

  if (width == 0 || height == 0) {
    return sf::Image({1u, 1u}, sf::Color::Transparent);
  }

  sf::RenderTexture rt({width, height});
  rt.clear(sf::Color::Transparent);

  sfText.setPosition({-bounds.position.x, -bounds.position.y});
  rt.draw(sfText);
  rt.display();

  return rt.getTexture().copyToImage();
}
