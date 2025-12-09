#include "game_mechanics/npc.h"

#include "components.h"
#include "core/camera.h"
#include "ecs/components.h"
#include "ecs/systems.h"
#include "random/random_positions.h"

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

entt::entity spawnMinotaurInRing(entt::registry &registry,
    unsigned int maxHp,
    unsigned int collisionDamage,
    const sf::Vector2f &playerPos,
    float innerRadius,
    float outerRadius,
    int worldWidth,
    int worldHeight) {
  sf::Vector2f spawnPos;
  float innerSq = innerRadius * innerRadius;
  float outerSq = outerRadius * outerRadius;

  const int maxTries = 1024;
  int tries = 0;

  while (true) {
    const float margin = 1.f;
    spawnPos = randomPointOnMap(worldWidth, worldHeight, margin);
    sf::Vector2f diff = spawnPos - playerPos;
    float d2 = diff.x * diff.x + diff.y * diff.y;
    if (d2 >= innerSq && d2 <= outerSq)
      break;

    if (++tries >= maxTries) {
      spawnPos = playerPos + sf::Vector2f{outerRadius, 0.f};
      break;
    }
  }

  sf::Vector2f minoSize{60.f, 60.f};
  sf::IntRect minoRect({0, 0}, {60, 60});

  std::unordered_map<int, engine::AnimationClip> npcClips = {
      {0, {"assets/npc/minotaur_idle.png", 12, 0.08f, minoRect}},
      {1, {"assets/npc/minotaur_walk.png", 18, 0.08f, minoRect}}};

  auto minotaur = systems::createNPC(registry, spawnPos, minoSize, npcClips, 60.f);
  registry.emplace<SideViewOnly>(minotaur);
  registry.emplace<engine::ChasingPlayer>(minotaur);
  registry.emplace<engine::CastsShadow>(minotaur);
  registry.emplace<HP>(minotaur, HP{maxHp, maxHp});
  registry.emplace<NpcCollisionDamage>(minotaur, NpcCollisionDamage{collisionDamage});
  registry.emplace<Solid>(minotaur, Solid{true});

  return minotaur;
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

unsigned int clearDeadNpc(entt::registry &registry) {
  auto npcView = registry.view<const HP, engine::ChasingPlayer>();

  unsigned int count = 0;
  for (auto npc : npcView) {
    const auto &hp = npcView.get<const HP>(npc);
    if (hp.current == 0) {
      registry.destroy(npc);
      ++count;
    }
  }
  return count;
}
