#pragma once

#include <SFML/System/Vector2.hpp>

// Returns a random point inside the map [0,width) x [0,height),
// keeping at least `margin` distance from each border when possible.
sf::Vector2f randomPointOnMap(int width, int height, float margin);

