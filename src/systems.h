#pragma once

#include <entt/entt.hpp>
#include <vector>

#include "ecs/components.h"
#include "ecs/tile.h"

void gameMovementSystem(
    entt::registry &registry, std::vector<engine::Tile> &tiles, int worldWidth, int worldHeight, float dt);

void gameAnimationSystem(entt::registry &registry, float dt);
