#pragma once

#include "ecs/components.h"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <array>

struct HP {
  unsigned int current;
  unsigned int max;
};

struct NpcCollisionDamage {
  unsigned int damage;
};

struct LastDamageTime {
  double lastDamageTime = 0.0;
  double damageCooldown = 0.2; // in seconds
};

// Marks entity as solid for movement/collision.
// Entities without this component (or with value == false) doens't block movement.
struct Solid {
  bool value = true;
};

struct SideViewOnly {};

struct UISprite {
  sf::Image *image;
  engine::Position pos;
  int zIndex = 0;
};

enum class WeaponKind {
  MagicStick,
  Sword,
};

enum class ProjectileType {
  Linear,
  Radial,
};

struct Projectile {
  ProjectileType type = ProjectileType::Linear;
  float radius = 0.f;
  unsigned int damage = 0;
  float lifetime = 0.f;
  float maxLifetime = 1.f;
};

struct Weapon {
  WeaponKind kind = WeaponKind::MagicStick;
  ProjectileType type = ProjectileType::Linear;
  float radius = 0.f;
  float cooldown = 0.f;
  float cooldownRemaining = 0.f;
  unsigned int shotsPerAttack = 1;
  float shotInterval = 0.f;
  unsigned int shotsPending = 0;
  float shotTimer = 0.f;
  unsigned int damage = 0;
  float projectileSpeed = 0.f;
};

struct Weapons {
  std::array<Weapon, 2> slots;
};
