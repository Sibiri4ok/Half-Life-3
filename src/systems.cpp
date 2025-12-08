#include "systems.h"

#include "components.h"
#include "core/camera.h"
#include "ecs/components.h"
#include <cmath>

// Check if two entities are intersecting based on their positions and renderable sizes
// Gets screen positions
bool isEntitiesIntersecting(const sf::Vector2f &posA,
    const engine::Renderable &renderA,
    const sf::Vector2f &posB,
    const engine::Renderable &renderB,
    engine::Camera &camera) {
  float wA = renderA.targetSize.x;
  float hA = renderA.targetSize.y;
  float wB = renderB.targetSize.x;
  float hB = renderB.targetSize.y;

  sf::FloatRect rectA({posA.x - wA * 0.25f, posA.y}, {wA * 0.5f, hA * 0.3f});
  sf::FloatRect rectB({posB.x - wB * 0.25f, posB.y}, {wB * 0.5f, hB * 0.3f});

  return rectA.findIntersection(rectB).has_value();
}

void gameMovementSystem(entt::registry &registry,
    std::vector<engine::Tile> &tiles,
    int worldWidth,
    int worldHeight,
    float dt,
    engine::Camera &camera) {
  auto view = registry.view<engine::Position, const engine::Velocity, const engine::Renderable>();
  auto getIndex = [&](int x, int y) { return y * worldWidth + x; };

  for (auto entity : view) {
    auto &pos = view.get<engine::Position>(entity);
    const auto &vel = view.get<const engine::Velocity>(entity);
    const auto &render = view.get<const engine::Renderable>(entity);
    const auto &targetSize = render.targetSize;

    sf::Vector2f deltaScreen = vel.value * dt;
    sf::Vector2f deltaWorld = camera.screenToWorld(deltaScreen);

    // Get world position
    auto withinMap = [&](float newX, float newY) {
      int tileX = static_cast<int>(std::floor(newX)) - 1;
      int tileY = static_cast<int>(std::floor(newY));

      if (tileX < 0 || tileX >= worldWidth || tileY < 0 || tileY >= worldHeight)
        return false;

      return !tiles[getIndex(tileX, tileY)].solid;
    };

    // Check collision with other entities, gets screen position
    auto blockedByAnother = [&](const sf::Vector2f &posScreen, const sf::Vector2f &newPosScreen) {
      sf::Vector2f move = newPosScreen - posScreen;

      for (auto otherEntity : view) {
        if (otherEntity == entity)
          continue;

        const auto &otherPos = view.get<engine::Position>(otherEntity);
        const auto &otherRender = view.get<const engine::Renderable>(otherEntity);
        sf::Vector2f otherScreen = camera.worldToScreen(otherPos.value);

        sf::Vector2f toOther = otherScreen - posScreen;
        if (move.x * toOther.x + move.y * toOther.y <= 0.f)
          continue;

        if (isEntitiesIntersecting(newPosScreen, render, otherScreen, otherRender, camera))
          return true;
      }
      return false;
    };

    sf::Vector2f newPos = pos.value;
    auto screenPos = camera.worldToScreen(pos.value);
    auto anchorPos = camera.screenToWorld({screenPos.x, screenPos.y + targetSize.y * 0.4f});
    if (withinMap(anchorPos.x + deltaWorld.x, anchorPos.y) &&
        !blockedByAnother({screenPos.x, screenPos.y}, {screenPos.x + deltaScreen.x, screenPos.y})) {
      pos.value.x += deltaWorld.x;
      anchorPos.x += deltaWorld.x;
    }
    screenPos = camera.worldToScreen(pos.value);
    if (withinMap(anchorPos.x, anchorPos.y + deltaWorld.y) &&
        !blockedByAnother({screenPos.x, screenPos.y}, {screenPos.x, screenPos.y + deltaScreen.y})) {
      anchorPos.y += deltaWorld.y;
      pos.value.y += deltaWorld.y;
    }
  }
}

void gameAnimationSystem(entt::registry &registry, float dt) {
  auto view = registry.view<engine::Animation, engine::Velocity, engine::Renderable>();

  for (auto entity : view) {
    auto &anim = view.get<engine::Animation>(entity);
    auto &vel = view.get<engine::Velocity>(entity);
    auto &render = view.get<engine::Renderable>(entity);

    float moving = sqrtf(vel.value.x * vel.value.x + vel.value.y * vel.value.y);
    engine::Direction newDir = anim.direction;
    int newState = moving > 0.0f ? 1 : 0; // 0 - idle, 1 - walk

    auto it = anim.clips.find(newState);
    if (it == anim.clips.end())
      continue;

    const auto &clip = it->second;
    if (clip.frameCount <= 1)
      continue;

    anim.frameTime += dt;

    bool sideOnly = registry.all_of<SideViewOnly>(entity);
    if (!moving) {
      newDir = anim.direction;
    } else if (std::abs(vel.value.y) > std::abs(vel.value.x) && !sideOnly) {
      newDir = vel.value.y > 0.f ? engine::Direction::Down : engine::Direction::Up;
    } else if (std::abs(vel.value.y) > std::abs(vel.value.x) && sideOnly) {
      newDir = vel.value.x > 0.f ? engine::Direction::Left : engine::Direction::Right;
    } else if (std::abs(vel.value.x) > std::abs(vel.value.y)) {
      newDir = vel.value.x > 0.f ? engine::Direction::Left : engine::Direction::Right;
    }

    if (anim.direction != newDir || anim.state != newState) {
      anim.state = newState;
      anim.direction = newDir;
      anim.row = static_cast<int>(newDir);
      anim.frameIdx = 0;
      anim.frameTime = 0.f;
    }

    while (anim.frameTime >= clip.frameDuration) {
      anim.frameTime -= clip.frameDuration;
      anim.frameIdx = (anim.frameIdx + 1) % clip.frameCount;
    }
  }
}

void gameInputSystem(entt::registry &registry, const engine::Input &input) {
  auto view = registry.view<engine::Velocity, engine::Speed, engine::PlayerControlled, engine::Animation>();

  for (auto entity : view) {
    auto &vel = view.get<engine::Velocity>(entity);
    auto &anim = view.get<engine::Animation>(entity);
    auto &speed = view.get<engine::Speed>(entity);

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
      vel.value *= speed.value;
    }
  }
}
