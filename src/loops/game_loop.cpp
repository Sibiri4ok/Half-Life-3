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
#include "game_mechanics/npc.h"
#include "game_mechanics/weapons.h"
#include "render/colorDmg.h"
#include "render/textToImage.h"
#include "render/ui_render.h"
#include "render/weapon_textures.h"
#include "resources/image_manager.h"
#include "systems.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>

const unsigned int FPS_TEXT_SIZE = 30;
const unsigned int HP_TEXT_SIZE = 30;
const unsigned int TIMER_TEXT_SIZE = 40;

const unsigned int LEVEL_TIME = 300; // seconds

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

  sf::Vector2f mainSize{56.f, 60.f};
  sf::IntRect mainRect({0, 0}, {56, 60});

  std::unordered_map<int, engine::AnimationClip> mainHeroClips = {
      {0, {"assets/npc/main_idle.png", 12, 0.15f, mainRect}},
      {1, {"assets/npc/main_walk.png", 6, 0.08f, mainRect}},
  };

  auto main_hero = systems::createNPC(m_registry, {2.f, 2.f}, mainSize, mainHeroClips, 200.f);
  m_registry.emplace<engine::PlayerControlled>(main_hero);
  m_registry.emplace<engine::CastsShadow>(main_hero);
  HP hp = {100, 100};
  m_registry.emplace<HP>(main_hero, hp);
  m_registry.emplace<Solid>(main_hero, Solid{true});
  m_registry.emplace<LastDamageTime>(main_hero, LastDamageTime{0.0});

  Weapons playerWeapons{};
  playerWeapons.slots[0] = makeLinearWeapon(WeaponKind::MagicStick, 7.f, 2.0f, 1, 0.1f, 8, 400.f);
  playerWeapons.slots[1] = makeRadialWeapon(WeaponKind::Sword, 3.f, 1.5f, 2, 0.1f, 5);

  // Procedural weapon textures; ring size follows sword radius.
  const unsigned magicBallTexSize = 32u;
  const float swordPixelsPerRadius = 64.f;
  unsigned swordRingTexSize = static_cast<unsigned>(playerWeapons.slots[1].radius * swordPixelsPerRadius);
  if (swordRingTexSize == 0u)
    swordRingTexSize = 1u;
  render::generateWeaponTextures(magicBallTexSize, swordRingTexSize);

  m_registry.emplace<Weapons>(main_hero, playerWeapons);

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
    m_registry.emplace<NpcCollisionDamage>(minotaur, NpcCollisionDamage{10});
    m_registry.emplace<Solid>(minotaur, Solid{true});
  }

  if (!uiFont.openFromFile("fonts/DejaVuSans.ttf")) {
    throw std::runtime_error("Failed to load font for UI");
  }

  uiEntities.fps = m_registry.create();
  uiEntities.hp = m_registry.create();
  uiEntities.timer = m_registry.create();

  uiAssets.fps = textToImage("FPS 0", uiFont, FPS_TEXT_SIZE, sf::Color::White);
  std::string hpText = "HP " + std::to_string(hp.current) + "/" + std::to_string(hp.max);
  uiAssets.hp = textToImage(hpText, uiFont, HP_TEXT_SIZE, sf::Color::Red);
  uiAssets.timer = textToImage("5:00", uiFont, TIMER_TEXT_SIZE, sf::Color::White);

  UISprite ui{};
  ui.image = &uiAssets.fps;
  ui.pos = engine::Position{sf::Vector2f{10.f, 10.f}};
  UISprite uiHP{};
  uiHP.image = &uiAssets.hp;
  uiHP.pos = engine::Position{sf::Vector2f{10.f, 40.f}};
  UISprite uiTimer{};
  uiTimer.image = &uiAssets.timer;
  uiTimer.pos = engine::Position{sf::Vector2f{m_engine->camera.size.x / 2.f - 60.f, 10.f}};

  m_registry.emplace<UISprite>(uiEntities.fps, ui);
  m_registry.emplace<UISprite>(uiEntities.hp, uiHP);
  m_registry.emplace<UISprite>(uiEntities.timer, uiTimer);
}

void GameLoop::update(engine::Input &input, float dt) {
  levelTimer += dt;
  logicTimer += dt;
  uiTimer += dt;
  const double alpha = 0.1;
  emaDeltaTime = alpha * dt + (1.0 - alpha) * emaDeltaTime;
  updateUI();

  engine::Camera &camera = m_engine->camera;
  gameInputSystem(m_registry, input);
  gameNpcFollowPlayerSystem(m_registry, camera);
  gameWeaponSystem(m_registry, dt, m_engine->camera);
  gameMovementSystem(m_registry, tiles, width, height, dt, levelTimer, m_engine->camera);
  gameProjectileDamageSystem(m_registry, dt, m_engine->camera);
  gameAnimationSystem(m_registry, dt);
  updatePlayerDamageColor(m_registry, logicTimer);
  clearDeadNpc(m_registry);

  auto playerView = m_registry.view<const engine::Position, const engine::PlayerControlled>();
  for (auto entity : playerView) {
    const auto &pos = playerView.get<const engine::Position>(entity);
    m_engine->camera.position = m_engine->camera.worldToScreen(pos.value);
  }
}

void GameLoop::collectRenderData(engine::RenderFrame &frame, engine::Camera &camera) {
  frame.tileVertices = m_staticMapPoints;
  systems::renderSystem(m_registry, frame, camera, m_engine->imageManager);
  uiRender(m_registry, frame, camera);
}

bool GameLoop::isFinished() const { return m_finished; }

int GameLoop::getEmaFps() { return static_cast<int>(emaDeltaTime == 0.0 ? 60.0 : 1.0 / emaDeltaTime); }

sf::Image GameLoop::timerImage() {
  int timeLeft = static_cast<int>(LEVEL_TIME - levelTimer);
  if (timeLeft < 0)
    timeLeft = 0;
  int minutes = timeLeft / 60;
  int seconds = timeLeft % 60;
  char text[6];
  std::snprintf(text, sizeof(text), "%d:%02d", minutes, seconds);
  return textToImage(std::string(text), uiFont, TIMER_TEXT_SIZE, sf::Color::White);
}

void GameLoop::updateUI() {
  if (uiTimer < 0.3) {
    return;
  }
  uiAssets.fps = textToImage("FPS " + std::to_string(getEmaFps()), uiFont, FPS_TEXT_SIZE, sf::Color::White);
  uiAssets.timer = timerImage();

  auto playerView = m_registry.view<const engine::PlayerControlled, const HP>();
  HP hp = playerView.get<HP>(*(playerView.begin()));
  uiAssets.hp = textToImage("HP " + std::to_string(hp.current) + "/" + std::to_string(hp.max),
      uiFont,
      HP_TEXT_SIZE,
      sf::Color::Red);
  uiTimer = 0.0;
  return;
}
