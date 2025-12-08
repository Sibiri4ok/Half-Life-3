#include "ui_render.h"

#include "components.h"
#include "core/camera.h"
#include "core/render_frame.h"
#include "ecs/components.h"

void uiRender(entt::registry &registry, engine::RenderFrame &frame, const engine::Camera &camera) {
  sf::Vector2f viewTopLeft{
      camera.position.x - camera.size.x * 0.5f, camera.position.y - camera.size.y * 0.5f};

  auto view = registry.view<const engine::Position, const UISprite>();

  std::vector<entt::entity> entities(view.begin(), view.end());
  std::sort(entities.begin(), entities.end(), [&](entt::entity a, entt::entity b) {
    const auto &ua = view.get<const UISprite>(a);
    const auto &ub = view.get<const UISprite>(b);
    return ua.zIndex < ub.zIndex;
  });

  for (auto entity : entities) {
    const auto &pos = view.get<const engine::Position>(entity);
    const auto &ui = view.get<const UISprite>(entity);
    if (!ui.image)
      continue;

    engine::RenderFrame::SpriteData sprite{};
    sprite.image = ui.image;
    sprite.textureRect = ui.rect;
    sprite.position = {viewTopLeft.x + pos.value.x, viewTopLeft.y + pos.value.y}; // top left
    sprite.scale = {1.f, 1.f};
    sprite.rotation = sf::Angle::Zero;

    frame.sprites.push_back(sprite);
  }
}
