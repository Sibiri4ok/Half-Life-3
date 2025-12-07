#include "game_loop.h"

#include <random>

#include "components.h"
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
  m_engine->camera.size = {1200.f, 800.f};
  m_engine->render.getWindow().setSize({1200u, 800u});

  sf::Vector2f worldCenter = {width / 2.0f, height / 2.0f};
  sf::Vector2f screenCenter = m_engine->camera.worldToScreen(worldCenter);
  m_engine->camera.position = screenCenter;

  // Create and generate world tiles once

  const int tileWidth = 64.f;
  const int tileHeight = 32.f;
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

  // Create player
  sf::Vector2f mainSize{56.f, 60.f};
  sf::IntRect mainRect({0, 0}, {56, 60});

  std::unordered_map<int, engine::AnimationClip> mainHeroClips = {
      {0, {"assets/npc/main_idle.png", 12, 0.15f, mainRect}},
      {1, {"assets/npc/main_walk.png", 6, 0.08f, mainRect}},
  };

  auto main_hero = systems::createNPC(m_registry, {2.f, 2.f}, mainSize, mainHeroClips, 200.f);
  m_registry.emplace<engine::PlayerControlled>(main_hero);
  m_registry.emplace<engine::CastsShadow>(main_hero);
  m_registry.emplace<HP>(main_hero, HP{100, 100});

  // Create minotaurs
  for (int i = 0; i < 3; ++i) {
    sf::Vector2f minoSize{60.f, 60.f};
    sf::IntRect minoRect({0, 0}, {60, 60});

    std::unordered_map<int, engine::AnimationClip> npcClips = {
        {0, {"assets/npc/minotaur_idle.png", 12, 0.08f, minoRect}},
        {1, {"assets/npc/minotaur_walk.png", 18, 0.08f, minoRect}}};
    auto minotaur =
        systems::createNPC(m_registry, {float(i * 2 + 1), float(i * 2 + 1)}, minoSize, npcClips, 60.f);
    m_registry.emplace<SideViewOnly>(minotaur);
    m_registry.emplace<engine::ChasingPlayer>(minotaur);
    m_registry.emplace<engine::CastsShadow>(minotaur);
    m_registry.emplace<HP>(minotaur, HP{20, 20});
  }
}

void GameLoop::update(engine::Input &input, float dt) {
  engine::Camera &camera = m_engine->camera;
  gameInputSystem(m_registry, input);
  gameNpcFollowPlayerSystem(m_registry, camera);
  gameMovementSystem(m_registry, tiles, width, height, dt, m_engine->camera);
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
