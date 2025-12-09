#pragma once

#include "components.h"

inline Weapon makeLinearWeapon(WeaponKind kind,
    float radius,
    float cooldown,
    unsigned int shotsPerAttack,
    float shotInterval,
    unsigned int damage,
    float projectileSpeed) {
  Weapon w{};
  w.kind = kind;
  w.type = ProjectileType::Linear;
  w.radius = radius;
  w.cooldown = cooldown;
  w.shotsPerAttack = shotsPerAttack;
  w.shotInterval = shotInterval;
  w.damage = damage;
  w.projectileSpeed = projectileSpeed;
  return w;
}

inline Weapon makeRadialWeapon(WeaponKind kind,
    float radius,
    float cooldown,
    unsigned int shotsPerAttack,
    float shotInterval,
    unsigned int damage) {
  Weapon w{};
  w.kind = kind;
  w.type = ProjectileType::Radial;
  w.radius = radius;
  w.cooldown = cooldown;
  w.shotsPerAttack = shotsPerAttack;
  w.shotInterval = shotInterval;
  w.damage = damage;
  w.projectileSpeed = 0.f;
  return w;
}
