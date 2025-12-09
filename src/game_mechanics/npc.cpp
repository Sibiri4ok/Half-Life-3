#include "game_mechanics/npc.h"

#include "components.h"
#include "core/camera.h"
#include "ecs/components.h"

#include <SFML/System/Vector2.hpp>
#include <cassert>
#include <cmath>
#include <entt/entt.hpp>
#include <unordered_map>

entt::entity gameCreateNPC(entt::registry &registry,
    const sf::Vector2f &pos,
    const sf::Vector2f &targetSize,
    float speed,
    unsigned int hp,
    const std::unordered_map<int, engine::AnimationClip> &clips) {
  assert(!clips.empty() && "NPC must have at least one animation clip!");

  auto e = registry.create();
  registry.emplace<engine::Position>(e, pos);
  registry.emplace<engine::Speed>(e, speed);
  registry.emplace<engine::Velocity>(e);
  registry.emplace<HP>(e, HP{hp, hp});

  engine::Renderable render;
  render.textureName = clips.begin()->second.texture;
  render.textureRect = clips.begin()->second.frameRect;
  render.targetSize = targetSize;
  registry.emplace<engine::Renderable>(e, std::move(render));
  engine::Animation anim;
  anim.clips = clips;
  anim.state = clips.begin()->first;
  registry.emplace<engine::Animation>(e, std::move(anim));
  return e;
}

void gameNpcFollowPlayerSystem(entt::registry &registry, engine::Camera &camera) {
  auto playerView = registry.view<const engine::Position, const engine::PlayerControlled>();
  const auto playerEntity = *playerView.begin();
  const auto &playerPos = playerView.get<const engine::Position>(playerEntity);

  auto npcView =
      registry.view<const engine::Position, engine::Velocity, engine::Speed, engine::ChasingPlayer>();

  for (auto npc : npcView) {
    const auto &pos = npcView.get<const engine::Position>(npc);
    auto &vel = npcView.get<engine::Velocity>(npc);
    const auto &speed = npcView.get<const engine::Speed>(npc);

    sf::Vector2f playerScreen = camera.worldToScreen(playerPos.value);
    sf::Vector2f npcScreen = camera.worldToScreen(pos.value);
    sf::Vector2f diff = playerScreen - npcScreen;

    float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    if (len > 1.f) {
      sf::Vector2f dir = diff / len;
      vel.value = dir * speed.value;
    } else {
      vel.value = {0.f, 0.f};
    }
  }
}

void clearDeadNpc(entt::registry &registry) {
  auto npcView = registry.view<const HP, engine::ChasingPlayer>();

  for (auto npc : npcView) {
    const auto &hp = npcView.get<const HP>(npc);
    if (hp.current == 0) {
      registry.destroy(npc);
    }
  }
}
