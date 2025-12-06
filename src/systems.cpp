#include "systems.h"

#include "ecs/components.h"
#include <cmath>

void gameMovementSystem(
    entt::registry &registry, std::vector<engine::Tile> &tiles, int worldWidth, int worldHeight, float dt) {
  auto view = registry.view<engine::Position, const engine::Velocity, const engine::Speed>();
  auto getIndex = [&](int x, int y) { return y * worldWidth + x; };

  for (auto entity : view) {
    auto &pos = view.get<engine::Position>(entity);
    const auto &vel = view.get<const engine::Velocity>(entity);
    const auto &speed = view.get<const engine::Speed>(entity);

    sf::Vector2f newPos = pos.value;
    sf::Vector2f delta = vel.value * speed.value * dt;

    auto canMove = [&](float newX, float newY) {
      int tileX = static_cast<int>(std::floor(newX)) - 1;
      int tileY = static_cast<int>(std::floor(newY));

      if (tileX < 0 || tileX >= worldWidth || tileY < 0 || tileY >= worldHeight)
        return false;

      return !tiles[getIndex(tileX, tileY)].solid;
    };

    if (canMove(pos.value.x + delta.x, pos.value.y))
      newPos.x += delta.x;
    if (canMove(newPos.x, pos.value.y + delta.y))
      newPos.y += delta.y;

    pos.value = newPos;
  }
}

void gameAnimationSystem(entt::registry &registry, float dt) {
  auto view = registry.view<engine::Animation, engine::Velocity, engine::Renderable>();

  for (auto entity : view) {
    auto &anim = view.get<engine::Animation>(entity);
    auto &vel = view.get<engine::Velocity>(entity);
    auto &render = view.get<engine::Renderable>(entity);

    int newState = (std::sqrt(vel.value.x * vel.value.x + vel.value.y * vel.value.y) > 0.1f) ? 1 : 0;

    if (anim.clips.find(newState) != anim.clips.end() && anim.state != newState) {
      anim.state = newState;
      anim.frameIdx = 0;
      anim.frameTime = 0.f;
    }
  }
}
