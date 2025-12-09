#include "random/random_positions.h"

#include <algorithm>
#include <random>

sf::Vector2f randomPointOnMap(int width, int height, float margin) {
  if (width <= 0 || height <= 0)
    return {0.f, 0.f};

  float minX = std::max(0.f, margin);
  float minY = std::max(0.f, margin);
  float maxX = std::max(minX + 0.001f, static_cast<float>(width) - margin);
  float maxY = std::max(minY + 0.001f, static_cast<float>(height) - margin);

  static std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> distX(minX, maxX);
  std::uniform_real_distribution<float> distY(minY, maxY);

  return {distX(rng), distY(rng)};
}

