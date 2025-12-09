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
    double levelTime,
    engine::Camera &camera);

void gameAnimationSystem(entt::registry &registry, float dt);
void gameInputSystem(entt::registry &registry, const engine::Input &input, float &gameSpeed);

// Handles all player weapons (projectile + radial) in a single system.
void gameWeaponSystem(entt::registry &registry, float dt, engine::Camera &camera);

// Updates projectiles (lifetime) and applies their damage to NPCs.
void gameProjectileDamageSystem(entt::registry &registry, float dt, engine::Camera &camera);
