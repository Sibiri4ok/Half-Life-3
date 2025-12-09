#include "systems.h"

#include "components.h"
#include "core/camera.h"
#include "ecs/components.h"
#include "render/weapon_textures.h"
#include <cmath>

float lengthSquared(const sf::Vector2f &v) { return v.x * v.x + v.y * v.y; }

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

bool isWeaponHitEntity(const sf::Vector2f &projScreen,
    float projRadius,
    const sf::Vector2f &npcScreen,
    const engine::Renderable &npcRender) {
  float w = npcRender.targetSize.x;
  float h = npcRender.targetSize.y;

  // Projectile: simple circle approximated by square.
  sf::FloatRect projRect(
      {projScreen.x - projRadius, projScreen.y - projRadius}, {projRadius * 2.f, projRadius * 2.f});

  // NPC hitbox: X in [0.1; 0.9], Y in [0.1; 0.9].
  sf::FloatRect npcRect({npcScreen.x - w * 0.4f, npcScreen.y - h * 0.1f}, {w * 0.8f, h * 0.8f});

  return projRect.findIntersection(npcRect).has_value();
}

static void applyNpcCollisionDamage(
    entt::registry &registry, entt::entity npc, entt::entity player, double currentTime) {
  if (!registry.all_of<engine::ChasingPlayer, NpcCollisionDamage>(npc))
    return;
  if (!registry.all_of<engine::PlayerControlled, HP, LastDamageTime>(player))
    return;

  auto &dmgTime = registry.get<LastDamageTime>(player);
  if (currentTime - dmgTime.lastDamageTime < dmgTime.damageCooldown)
    return;

  auto &damage = registry.get<NpcCollisionDamage>(npc);
  auto &hp = registry.get<HP>(player);

  hp.current = hp.current > damage.damage ? hp.current - damage.damage : 0;
  dmgTime.lastDamageTime = currentTime;
}

static void spawnLinearProjectile(entt::registry &registry,
    engine::Camera &camera,
    const sf::Vector2f &originPos,
    const sf::Vector2f &targetPos,
    const Weapon &weapon) {
  sf::Vector2f originScreen = camera.worldToScreen(originPos);
  sf::Vector2f targetScreen = camera.worldToScreen(targetPos);
  sf::Vector2f dir = targetScreen - originScreen;
  float lenSq = lengthSquared(dir);
  if (lenSq <= 0.0001f)
    return;

  float len = std::sqrt(lenSq);
  dir /= len;

  const float startOffsetScreen = 20.f;
  sf::Vector2f originScreenShifted = originScreen + dir * startOffsetScreen;
  sf::Vector2f offsetWorld = camera.screenToWorld(originScreenShifted) - camera.screenToWorld(originScreen);
  sf::Vector2f startWorldPos = originPos + offsetWorld;

  auto e = registry.create();
  registry.emplace<engine::Position>(e, startWorldPos);
  registry.emplace<engine::Velocity>(
      e, sf::Vector2f{dir.x * weapon.projectileSpeed, dir.y * weapon.projectileSpeed});

  // Simple magic ball projectile.
  engine::Renderable render;
  render.textureName = std::string(render::MAGIC_BALL_TEXTURE);
  render.textureRect = sf::IntRect({0, 0}, {32, 32});
  render.targetSize = {18.f, 18.f};
  render.color = sf::Color::White;
  registry.emplace<engine::Renderable>(e, std::move(render));

  Projectile proj;
  proj.type = ProjectileType::Linear;
  proj.radius = 0.4f;
  proj.damage = weapon.damage;
  proj.lifetime = 0.f;
  proj.maxLifetime = 2.f;
  registry.emplace<Projectile>(e, proj);
}

static void spawnRadialEffect(entt::registry &registry,
    engine::Camera &camera,
    const sf::Vector2f &originPos,
    const Weapon &weapon,
    float radiusFactor) {
  auto e = registry.create();

  sf::Vector2f heroScreen = camera.worldToScreen(originPos);

  // Visual size scales with weapon.radius, slightly larger than actual damage radius.
  constexpr float baseTextureSize = 64.f;  // sword_ring.png generated as 64x64
  constexpr float visualBaseRadius = 1.5f; // visual calibration in world units

  float scale = (weapon.radius * radiusFactor) / visualBaseRadius;
  if (scale < 0.5f)
    scale = 0.5f;

  float sizePixels = baseTextureSize * scale;
  sf::Vector2f anchorScreen = heroScreen + sf::Vector2f{0.f, sizePixels * 0.5f};
  sf::Vector2f ringWorld = camera.screenToWorld(anchorScreen);

  registry.emplace<engine::Position>(e, ringWorld);
  registry.emplace<engine::Velocity>(e, sf::Vector2f{0.f, 0.f});

  engine::Renderable render;
  render.textureName = std::string(render::SWORD_RING_TEXTURE);
  render.textureRect =
      sf::IntRect({0, 0}, {static_cast<int>(baseTextureSize), static_cast<int>(baseTextureSize)});
  render.targetSize = {sizePixels, sizePixels};
  render.color = sf::Color(255, 255, 255, 230);
  registry.emplace<engine::Renderable>(e, std::move(render));

  Projectile proj;
  proj.type = ProjectileType::Radial;
  proj.radius = weapon.radius * radiusFactor;
  proj.damage = 0;
  proj.lifetime = 0.f;
  proj.maxLifetime = 0.35f;
  registry.emplace<Projectile>(e, proj);
}

static void applyRadialDamage(
    entt::registry &registry, const sf::Vector2f &origin, float radius, unsigned int damage) {
  float radiusSq = radius * radius;
  auto enemies = registry.view<engine::Position, HP>();

  for (auto enemy : enemies) {
    if (registry.all_of<engine::PlayerControlled>(enemy))
      continue;

    auto &hp = enemies.get<HP>(enemy);
    const auto &pos = enemies.get<engine::Position>(enemy);

    sf::Vector2f diff = pos.value - origin;
    if (lengthSquared(diff) <= radiusSq) {
      hp.current = hp.current > damage ? hp.current - damage : 0;
    }
  }
}

static entt::entity findNearestEnemy(entt::registry &registry, const sf::Vector2f &origin, float radius) {
  float radiusSq = radius * radius;
  auto enemies = registry.view<engine::Position, HP>();

  entt::entity best = entt::null;
  float bestDistSq = radiusSq;

  for (auto enemy : enemies) {
    if (registry.all_of<engine::PlayerControlled>(enemy))
      continue;

    const auto &pos = enemies.get<engine::Position>(enemy);
    sf::Vector2f diff = pos.value - origin;
    float d2 = lengthSquared(diff);
    if (d2 <= bestDistSq) {
      bestDistSq = d2;
      best = enemy;
    }
  }

  return best;
}

void gameMovementSystem(entt::registry &registry,
    std::vector<engine::Tile> &tiles,
    int worldWidth,
    int worldHeight,
    float dt,
    double levelTime,
    engine::Camera &camera) {
  auto view = registry.view<engine::Position, const engine::Velocity, const engine::Renderable>();
  auto getIndex = [&](int x, int y) { return y * worldWidth + x; };

  for (auto entity : view) {
    bool isSolidMover = registry.all_of<Solid>(entity) && registry.get<Solid>(entity).value;

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

    // Check collision with other entities, gets screen position. If player collides with enemy, apply damage
    auto blockedByAnother = [&](const sf::Vector2f &posScreen, const sf::Vector2f &newPosScreen) {
      if (!isSolidMover)
        return false;

      sf::Vector2f move = newPosScreen - posScreen;

      for (auto otherEntity : view) {
        if (otherEntity == entity)
          continue;

        bool isSolidOther = registry.all_of<Solid>(otherEntity) && registry.get<Solid>(otherEntity).value;
        if (!isSolidOther)
          continue;

        const auto &otherPos = view.get<engine::Position>(otherEntity);
        const auto &otherRender = view.get<const engine::Renderable>(otherEntity);
        sf::Vector2f otherScreen = camera.worldToScreen(otherPos.value);

        sf::Vector2f toOther = otherScreen - posScreen;
        if (move.x * toOther.x + move.y * toOther.y <= 0.f)
          continue;

        if (isEntitiesIntersecting(newPosScreen, render, otherScreen, otherRender, camera)) {
          applyNpcCollisionDamage(registry, entity, otherEntity, levelTime);
          applyNpcCollisionDamage(registry, otherEntity, entity, levelTime);
          return true;
        }
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

void gameWeaponSystem(entt::registry &registry, float dt, engine::Camera &camera) {
  auto playerView = registry.view<engine::Position, Weapons, engine::PlayerControlled>();

  for (auto entity : playerView) {
    auto &pos = playerView.get<engine::Position>(entity);
    auto &weapons = playerView.get<Weapons>(entity);

    auto fireShot = [&](Weapon &weapon, entt::entity presetTarget = entt::null) {
      if (weapon.type == ProjectileType::Linear) {
        entt::entity target = presetTarget;
        if (target == entt::null) {
          target = findNearestEnemy(registry, pos.value, weapon.radius);
          if (target == entt::null)
            return;
        }
        const auto &targetPos = registry.get<engine::Position>(target);
        spawnLinearProjectile(registry, camera, pos.value, targetPos.value, weapon);
      } else if (weapon.type == ProjectileType::Radial) {
        applyRadialDamage(registry, pos.value, weapon.radius, weapon.damage);
        spawnRadialEffect(registry, camera, pos.value, weapon, 1.0f);
      }
    };

    auto updateWeapon = [&](Weapon &weapon) {
      if (weapon.shotsPerAttack == 0)
        return;

      if (weapon.shotsPending > 0) {
        weapon.shotTimer -= dt;
        if (weapon.shotTimer > 0.f)
          return;

        fireShot(weapon);

        --weapon.shotsPending;
        if (weapon.shotsPending > 0) {
          weapon.shotTimer = weapon.shotInterval;
        } else {
          weapon.cooldownRemaining = weapon.cooldown;
        }
        return;
      }

      if (weapon.cooldownRemaining > 0.f) {
        weapon.cooldownRemaining -= dt;
        if (weapon.cooldownRemaining > 0.f)
          return;
        weapon.cooldownRemaining = 0.f;
      }

      entt::entity target = findNearestEnemy(registry, pos.value, weapon.radius);
      if (target == entt::null)
        return;

      weapon.shotsPending = weapon.shotsPerAttack;

      fireShot(weapon, target);

      --weapon.shotsPending;
      if (weapon.shotsPending > 0) {
        weapon.shotTimer = weapon.shotInterval;
      } else {
        weapon.cooldownRemaining = weapon.cooldown;
      }
    };

    updateWeapon(weapons.slots[0]);
    updateWeapon(weapons.slots[1]);
  }
}

void gameProjectileDamageSystem(entt::registry &registry, float dt, engine::Camera &camera) {
  auto projView = registry.view<engine::Position, Projectile>();

  std::vector<entt::entity> toDestroy;

  for (auto entity : projView) {
    auto &pos = projView.get<engine::Position>(entity);
    auto &proj = projView.get<Projectile>(entity);

    proj.lifetime += dt;
    if (proj.lifetime >= proj.maxLifetime) {
      toDestroy.push_back(entity);
      continue;
    }

    if (proj.type == ProjectileType::Linear) {
      auto npcView = registry.view<engine::Position, const engine::Renderable, HP>();

      sf::Vector2f projPosScreen = camera.worldToScreen(pos.value);

      for (auto npc : npcView) {
        if (registry.all_of<engine::PlayerControlled>(npc))
          continue;

        const auto &npcPos = npcView.get<engine::Position>(npc);
        const auto &npcRender = npcView.get<const engine::Renderable>(npc);

        if (isWeaponHitEntity(projPosScreen, proj.radius, camera.worldToScreen(npcPos.value), npcRender)) {
          auto &hp = npcView.get<HP>(npc);
          hp.current = hp.current > proj.damage ? hp.current - proj.damage : 0;
          toDestroy.push_back(entity);
          break;
        }
      }
    } else if (proj.type == ProjectileType::Radial) {
      auto &render = registry.get<engine::Renderable>(entity);
      float t = proj.lifetime / proj.maxLifetime;
      if (t < 0.f)
        t = 0.f;
      if (t > 1.f)
        t = 1.f;

      sf::Color c = render.color;
      float baseAlpha = 230.f;
      float alpha = baseAlpha * (1.f - t);
      if (alpha < 0.f)
        alpha = 0.f;
      c.a = static_cast<std::uint8_t>(alpha);
      render.color = c;
    }
  }

  for (auto e : toDestroy) {
    registry.destroy(e);
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

void gameInputSystem(entt::registry &registry, const engine::Input &input, float &gameSpeed) {
  auto view = registry.view<engine::Velocity, engine::Speed, engine::PlayerControlled, engine::Animation>();

  static bool plusPrev = false;
  static bool minusPrev = false;
  static bool escPrev = false;

  bool plusNow = input.isKeyDown(sf::Keyboard::Key::Equal);   // '=' / '+'
  bool minusNow = input.isKeyDown(sf::Keyboard::Key::Hyphen); // '-' / '_'
  bool escNow = input.isKeyDown(sf::Keyboard::Key::Escape);

  if (plusNow && !plusPrev) {
    float next = gameSpeed <= 0.f ? 1.f : gameSpeed + 1.f;
    if (next > 8.f)
      next = 8.f;
    gameSpeed = next;
  }
  if (minusNow && !minusPrev && gameSpeed > 0.f) {
    float next = gameSpeed - 1.f;
    if (next < 1.f)
      next = 1.f;
    gameSpeed = next;
  }
  if (escNow && !escPrev) {
    gameSpeed = (gameSpeed == 0.f) ? 1.f : 0.f;
  }

  plusPrev = plusNow;
  minusPrev = minusNow;
  escPrev = escNow;

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
