#pragma once

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Rect.hpp>

struct HP {
  unsigned int current;
  unsigned int max;
};

struct SideViewOnly {};

struct UISprite {
  const sf::Image *image = nullptr;
  sf::IntRect rect;
  int zIndex = 0;
};
