#include "ui_render.h"

#include "components.h"
#include "core/camera.h"
#include "core/render_frame.h"
#include "ecs/components.h"

void uiRender(entt::registry &registry, engine::RenderFrame &frame, const engine::Camera &camera) {
  sf::Vector2f viewTopLeft{
      camera.position.x - camera.size.x * 0.5f, camera.position.y - camera.size.y * 0.5f};

  auto view = registry.view<const UISprite>();

  std::vector<entt::entity> entities(view.begin(), view.end());
  std::sort(entities.begin(), entities.end(), [&](entt::entity a, entt::entity b) {
    const auto &ua = view.get<const UISprite>(a);
    const auto &ub = view.get<const UISprite>(b);
    return ua.zIndex < ub.zIndex;
  });

  for (auto entity : entities) {
    const auto &ui = view.get<const UISprite>(entity);
    const auto &pos = ui.pos;
    const sf::IntRect rect({0, 0},
        {static_cast<int>(ui.image->getSize().x), static_cast<int>(ui.image->getSize().y)}); // Full image
    if (!ui.image)
      continue;

    engine::RenderFrame::SpriteData sprite{};
    sprite.image = ui.image;
    sprite.textureRect = rect;
    sprite.position = {viewTopLeft.x + pos.value.x, viewTopLeft.y + pos.value.y}; // top left
    sprite.scale = {1.f, 1.f};
    sprite.rotation = sf::Angle::Zero;

    frame.sprites.push_back(sprite);
  }
}
