#include "render/colorDmg.h"
#include <entt/entt.hpp>

#include "components.h"
#include "ecs/components.h"
#include <SFML/Graphics/Color.hpp>

void updatePlayerDamageColor(entt::registry &registry, double currentTime) {
  auto view = registry.view<engine::Renderable, LastDamageTime, engine::PlayerControlled>();

  for (auto entity : view) {
    auto &render = view.get<engine::Renderable>(entity);
    auto &timing = view.get<LastDamageTime>(entity);

    const double elapsed = currentTime - timing.lastDamageTime;

    if (elapsed >= timing.damageCooldown * 2.0) {
      render.color = sf::Color::White;
      continue;
    }

    float t = 1.f - static_cast<float>(elapsed / timing.damageCooldown);
    if (t < 0.f)
      t = 0.f;
    if (t > 1.f)
      t = 1.f;

    const sf::Color base = sf::Color::White;
    const sf::Color hit = sf::Color(255, 80, 80); // лёгкий красный

    int r = static_cast<int>(base.r + (hit.r - base.r) * t);
    int g = static_cast<int>(base.g + (hit.g - base.g) * t);
    int b = static_cast<int>(base.b + (hit.b - base.b) * t);

    render.color = sf::Color(r, g, b, base.a);
  }
}
