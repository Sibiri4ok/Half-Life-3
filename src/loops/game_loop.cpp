#include "game_loop.h"

#include <random>

#include "cmath"
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

const unsigned int DEFAULT_UI_TEXT_SIZE = 30;
const unsigned int TIMER_TEXT_SIZE = 40;

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

  // Player
  sf::Vector2f mainSize{56.f, 60.f};
  sf::IntRect mainRect({0, 0}, {56, 60});

  std::unordered_map<int, engine::AnimationClip> mainHeroClips = {
      {0, {"assets/npc/main_idle.png", 12, 0.15f, mainRect}},
      {1, {"assets/npc/main_walk.png", 6, 0.08f, mainRect}},
  };

  sf::Vector2f playerStartPos{2.f, 2.f};
  auto main_hero = systems::createNPC(m_registry, playerStartPos, mainSize, mainHeroClips, 200.f);
  m_registry.emplace<engine::PlayerControlled>(main_hero);
  m_registry.emplace<engine::CastsShadow>(main_hero);
  HP hp{100, 100};
  m_registry.emplace<HP>(main_hero, hp);
  Experience exp{0, 0, 100};
  m_registry.emplace<Experience>(main_hero, exp);
  m_registry.emplace<Solid>(main_hero, Solid{true});
  m_registry.emplace<LastDamageTime>(main_hero, LastDamageTime{-1.0});

  // Weapons
  Weapons playerWeapons{};
  playerWeapons.slots[0] = makeLinearWeapon(WeaponKind::MagicStick,
      7.f,  // attack radius
      2.0f, // cooldown
      1,    // shots per attack
      0.1f, // shot interval
      8,    // dmg
      400.f // projectile speed
  );
  playerWeapons.slots[1] = makeRadialWeapon(WeaponKind::Sword,
      3.f,  // attack radius
      1.5f, // cooldown
      2,    // shots per attack
      0.1f, // shot interval
      5     // dmg
  );

  const unsigned magicBallTexSize = 32u;
  const float swordPixelsPerRadius = 64.f;
  unsigned swordRingTexSize = static_cast<unsigned>(playerWeapons.slots[1].radius * swordPixelsPerRadius);
  if (swordRingTexSize == 0u)
    swordRingTexSize = 1u;
  render::generateWeaponTextures(magicBallTexSize, swordRingTexSize);

  m_registry.emplace<Weapons>(main_hero, playerWeapons);

  // UI
  if (!uiFont.openFromFile("fonts/DejaVuSans.ttf")) {
    throw std::runtime_error("Failed to load font for UI");
  }

  uiEntities.hp = m_registry.create();
  uiEntities.exp = m_registry.create();
  uiEntities.kills = m_registry.create();
  uiEntities.timer = m_registry.create();
  uiEntities.fps = m_registry.create();

  std::string hpText = "HP " + std::to_string(hp.current) + "/" + std::to_string(hp.max);
  uiAssets.hp = textToImage(hpText, uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::Red);
  std::string expText = "Level " + std::to_string(exp.level) + " " + std::to_string(exp.currentXp) + "/" +
                        std::to_string(exp.xpToNextLevel);
  uiAssets.exp = textToImage(expText, uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::Cyan);
  uiAssets.kills = textToImage("Kills 0", uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::White);
  uiAssets.timer = textToImage("00:00", uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::White);
  uiAssets.fps = textToImage("FPS 0", uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::White);

  UISprite uiHP{};
  uiHP.image = &uiAssets.hp;
  uiHP.pos = engine::Position{sf::Vector2f{10.f, 10.f}};
  UISprite uiExp{};
  uiExp.image = &uiAssets.exp;
  uiExp.pos = engine::Position{sf::Vector2f{10.f, 40.f}};
  UISprite uiKills{};
  uiKills.image = &uiAssets.kills;
  uiKills.pos = engine::Position{sf::Vector2f{10.f, 70.f}};
  UISprite uiTimer{};
  uiTimer.image = &uiAssets.timer;
  uiTimer.pos = engine::Position{sf::Vector2f{m_engine->camera.size.x / 2.f - 60.f, 10.f}};
  UISprite uiFps{};
  uiFps.image = &uiAssets.fps;
  uiFps.pos = engine::Position{sf::Vector2f{m_engine->camera.size.x - 130.f, 10.f}};

  m_registry.emplace<UISprite>(uiEntities.hp, uiHP);
  m_registry.emplace<UISprite>(uiEntities.exp, uiExp);
  m_registry.emplace<UISprite>(uiEntities.kills, uiKills);
  m_registry.emplace<UISprite>(uiEntities.timer, uiTimer);
  m_registry.emplace<UISprite>(uiEntities.fps, uiFps);
}

void GameLoop::update(engine::Input &input, float dt) {
  globalTimer += dt;
  spawnTimer += dt;
  uiTimer += dt;
  const double alpha = 0.1;
  emaDeltaTime = alpha * dt + (1.0 - alpha) * emaDeltaTime;

  engine::Camera &camera = m_engine->camera;
  spawnMinotaurs();
  gameInputSystem(m_registry, input);
  gameNpcFollowPlayerSystem(m_registry, camera);
  gameWeaponSystem(m_registry, dt, m_engine->camera);
  gameMovementSystem(m_registry, tiles, width, height, dt, globalTimer, m_engine->camera);
  gameProjectileDamageSystem(m_registry, dt, m_engine->camera);
  gameAnimationSystem(m_registry, dt);
  updatePlayerDamageColor(m_registry, globalTimer);

  auto playerView = m_registry.view<const engine::Position, Experience, engine::PlayerControlled>();
  unsigned int killed = clearDeadNpc(m_registry);
  kills += killed;
  Experience &exp = playerView.get<Experience>(*(playerView.begin()));
  const unsigned int expPerKill = 10;
  exp.currentXp += killed * expPerKill;
  while (exp.currentXp >= exp.xpToNextLevel) {
    exp.currentXp -= exp.xpToNextLevel;
    exp.level += 1;
    exp.xpToNextLevel = static_cast<unsigned int>(exp.xpToNextLevel * 1.3f);
  }
  updateUI();

  // Move camera
  const auto &pos = playerView.get<const engine::Position>(*(playerView.begin()));
  m_engine->camera.position = m_engine->camera.worldToScreen(pos.value);
}

void GameLoop::collectRenderData(engine::RenderFrame &frame, engine::Camera &camera) {
  frame.tileVertices = m_staticMapPoints;
  systems::renderSystem(m_registry, frame, camera, m_engine->imageManager);
  uiRender(m_registry, frame, camera);
}

bool GameLoop::isFinished() const { return m_finished; }

int GameLoop::getEmaFps() { return static_cast<int>(emaDeltaTime == 0.0 ? 60.0 : 1.0 / emaDeltaTime); }

sf::Image GameLoop::timerImage() {
  int time = static_cast<int>(globalTimer);
  int minutes = time / 60;
  int seconds = time % 60;
  char text[6];
  std::snprintf(text, sizeof(text), "%02d:%02d", minutes, seconds);
  return textToImage(std::string(text), uiFont, TIMER_TEXT_SIZE, sf::Color::White);
}

void GameLoop::updateUI() {
  if (uiTimer < 0.3) {
    return;
  }
  auto playerView = m_registry.view<const engine::PlayerControlled, const HP, const Experience>();
  HP hp = playerView.get<HP>(*(playerView.begin()));
  uiAssets.hp = textToImage("HP " + std::to_string(hp.current) + "/" + std::to_string(hp.max),
      uiFont,
      DEFAULT_UI_TEXT_SIZE,
      sf::Color::Red);

  Experience exp = playerView.get<Experience>(*(playerView.begin()));
  std::string expText = "Level " + std::to_string(exp.level) + " " + std::to_string(exp.currentXp) + "/" +
                        std::to_string(exp.xpToNextLevel);
  uiAssets.exp = textToImage(expText, uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::Cyan);

  uiAssets.kills =
      textToImage("Kills " + std::to_string(kills), uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::White);
  uiAssets.timer = timerImage();
  uiAssets.fps =
      textToImage("FPS " + std::to_string(getEmaFps()), uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::White);

  uiTimer = 0.0;
  return;
}

void GameLoop::spawnMinotaurs() {
  auto playerView = m_registry.view<const engine::Position, const engine::PlayerControlled>();
  sf::Vector2f playerPos = playerView.get<const engine::Position>(*(playerView.begin())).value;

  int multiplier = globalTimer < 20.0 ? 1 : globalTimer / 20.0;
  int count = multiplier / 1.5;
  if (spawnTimer < 2.0) {
    return;
  }
  double usedTime = std::floor(spawnTimer);
  count *= usedTime / 2.0;
  count = std::max(1, count);
  spawnTimer -= usedTime;

  const unsigned int hp = 20 + (multiplier * 2);
  const unsigned int dmg = 10 + (multiplier * 2);
  for (int i = 0; i < count; ++i) {
    spawnMinotaurInRing(m_registry,
        hp,
        dmg,
        playerPos,
        4.f,  // inner spawn radius
        12.f, // outer spawn radius
        width,
        height);
  }
  return;
}
