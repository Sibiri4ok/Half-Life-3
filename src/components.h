#pragma once

#include "ecs/components.h"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Rect.hpp>

struct HP {
  unsigned int current;
  unsigned int max;
};

struct SideViewOnly {};

struct UISprite {
  sf::Image *image;
  engine::Position pos;
  int zIndex = 0;
};
