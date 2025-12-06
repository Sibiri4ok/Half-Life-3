#include "systems.h"

#include "core/camera.h"
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

void gameInputSystem(entt::registry &registry, const engine::Input &input, engine::Camera &camera) {
  auto view = registry.view<engine::Velocity, engine::PlayerControlled, engine::Animation>();

  for (auto entity : view) {
    auto &vel = view.get<engine::Velocity>(entity);
    auto &anim = view.get<engine::Animation>(entity);
    vel.value = {0.f, 0.f};

    if (input.isKeyDown(sf::Keyboard::Key::W)) {
      vel.value.y -= 1.f;
    }
    if (input.isKeyDown(sf::Keyboard::Key::S)) {
      vel.value.y += 1.f;
    }
    if (input.isKeyDown(sf::Keyboard::Key::A)) {
      vel.value.x -= 1.f;
    }
    if (input.isKeyDown(sf::Keyboard::Key::D)) {
      vel.value.x += 1.f;
    }

    float length = std::sqrt(vel.value.x * vel.value.x + vel.value.y * vel.value.y);
    if (length > 0.f) {
      vel.value /= length;
      vel.value = camera.screenToWorld(vel.value);

      if (std::abs(vel.value.x) > std::abs(vel.value.y)) {
        anim.row = (vel.value.x > 0.f) ? 1 : 2; // right=1, left=2
      } else {
        anim.row = (vel.value.y > 0.f) ? 0 : 3; // down=0, up=3
      }
    }
  }
}
