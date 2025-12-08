#pragma once

#include <SFML/System/Vector2.hpp>
#include <entt/entt.hpp>
#include <unordered_map>

namespace engine {
struct AnimationClip;
struct Camera;
struct Input;
} // namespace engine

entt::entity gameCreateNPC(entt::registry &registry,
    const sf::Vector2f &pos,
    const sf::Vector2f &targetSize,
    float speed,
    unsigned int hp,
    const std::unordered_map<int, engine::AnimationClip> &clips);

void gameNpcFollowPlayerSystem(entt::registry &registry, engine::Camera &camera);
