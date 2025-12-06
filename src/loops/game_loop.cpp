#include "game_loop.h"

#include <random>

#include "core/camera.h"
#include "core/engine.h"
#include "core/render.h"
#include "core/render_frame.h"
#include "ecs/components.h"
#include "ecs/systems.h"
#include "ecs/utils.h"
#include "ecs/world_loader.h"
#include "resources/image_manager.h"
#include "systems.h"

GameLoop::GameLoop() {
  engine::WorldLoader::loadWorldFromJson("assets/worlds/meadow.json", width, height, tileTextures, tiles);
}

void GameLoop::init() {
  m_engine = engine::Engine::get();
  sf::Vector2f worldCenter = {width / 2.0f, height / 2.0f};
  sf::Vector2f screenCenter = m_engine->camera.worldToScreen(worldCenter);
  m_engine->camera.position = screenCenter;

  // Create and generate world tiles once

  const int tileWidth = 32.f;
  const int tileHeight = 16.f;
  m_engine->camera.setTileSize(tileWidth, tileHeight);

  auto tileImages = engine::makeTileData(tileTextures, m_engine->imageManager);

  std::vector<engine::Tile> staticTiles = tiles;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const auto &tile = tiles[y * width + x];
      std::vector<int> groundLayers;

      for (int key : tile.layerIds) {
        const auto &texInfo = tileTextures.at(key);

        if (texInfo.is_ground) {
          groundLayers.push_back(key);
        } else {
          sf::Vector2f worldPos = {(float)x + 2.f, (float)y + 1.f};

          auto stObject = systems::createStaticObject(
              m_registry, worldPos, {32.f, 32.f}, texInfo.texture_src, sf::IntRect({0, 0}, {32, 32}));
          m_registry.emplace<engine::CastsShadow>(stObject);
        }
      }
      staticTiles[y * width + x].layerIds = std::move(groundLayers);
    }
  }

  m_engine->render.generateTileMapVertices(
      m_staticMapPoints, m_engine->camera, staticTiles, width, height, tileImages);

  // Create entities (player, NPC, etc.)

  sf::Vector2f targetWolfSize{64.f, 64.f};
  sf::IntRect frameRect({0, 0}, {64, 64});

  // Wolf
  std::unordered_map<int, engine::AnimationClip> wolfClips = {
      {0, {"assets/critters/wolf/wolf-idle.png", 4, 0.15f, frameRect}},
      {1, {"assets/critters/wolf/wolf-run.png", 8, 0.08f, frameRect}},
  };

  auto wolf = systems::createNPC(m_registry, {2.f, 2.f}, targetWolfSize, wolfClips, 5.f);
  m_registry.emplace<engine::PlayerControlled>(wolf);
  m_registry.emplace<engine::CastsShadow>(wolf);
}

void GameLoop::update(engine::Input &input, float dt) {
  systems::playerInputSystem(m_registry, input);
  systems::npcFollowPlayerSystem(m_registry, dt);
  systems::npcWanderSystem(m_registry, dt);
  gameMovementSystem(m_registry, tiles, width, height, dt);
  systems::animationSystem(m_registry, dt);
  gameAnimationSystem(m_registry, dt);

  // camera follow
  auto playerView = m_registry.view<const engine::Position, const engine::PlayerControlled>();
  for (auto entity : playerView) {
    const auto &pos = playerView.get<const engine::Position>(entity);
    m_engine->camera.position = m_engine->camera.worldToScreen(pos.value);
  }
}

void GameLoop::collectRenderData(engine::RenderFrame &frame, engine::Camera &camera) {
  // Collecting static map texture
  frame.tileVertices = m_staticMapPoints;

  // Collecting entities
  systems::renderSystem(m_registry, frame, camera, m_engine->imageManager);
}

bool GameLoop::isFinished() const { return m_finished; }
