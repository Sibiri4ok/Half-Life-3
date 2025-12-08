#pragma once

#include <entt/entt.hpp>

namespace engine {
struct Camera;
struct RenderFrame;
} // namespace engine

void uiRender(entt::registry &registry, engine::RenderFrame &frame, const engine::Camera &camera);
