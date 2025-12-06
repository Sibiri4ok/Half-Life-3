#pragma once

#include <entt/entt.hpp>
#include <vector>

#include "core/camera.h"
#include "core/input.h"
#include "ecs/components.h"
#include "ecs/tile.h"

void gameMovementSystem(entt::registry &registry,
    std::vector<engine::Tile> &tiles,
    int worldWidth,
    int worldHeight,
    float dt,
    engine::Camera &camera);

void gameAnimationSystem(entt::registry &registry, float dt);
void gameInputSystem(entt::registry &registry, const engine::Input &input);
