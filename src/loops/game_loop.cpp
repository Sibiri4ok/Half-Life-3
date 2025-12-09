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
#include "random/random_positions.h"
#include "render/colorDmg.h"
#include "render/textToImage.h"
#include "render/ui_render.h"
#include "render/weapon_textures.h"
#include "resources/image_manager.h"
#include "systems.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <algorithm>
#include <cstdio>

namespace {

struct UpgradeDef {
  UpgradeKind kind;
  const char *description;
};

const std::vector<UpgradeDef> ALL_UPGRADES = {
    {UpgradeKind::MoveSpeed, "+50 move speed"},
    {UpgradeKind::ExtraProjectiles, "+1 projectile for all weapons"},
    {UpgradeKind::Damage, "+5 damage for all weapons"},
    {UpgradeKind::Radius, "+100 radius for all weapons"},
    {UpgradeKind::Cooldown, "-10% cooldown for all weapons"},
    {UpgradeKind::MaxHp, "+30 HP (current and max)"},
    {UpgradeKind::Regen, "+40 HP regen per minute"},
    {UpgradeKind::XpGain, "+20% experience gain"},
    {UpgradeKind::MobCount, "+10% enemy count"},
};

} // namespace

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
      m_tileMeshes, m_engine->camera, staticTiles, width, height, tileImages);

  spawnStaticObjects(200);

  // Player
  sf::Vector2f mainSize{56.f, 60.f};
  sf::IntRect mainRect({0, 0}, {56, 60});

  std::unordered_map<int, engine::AnimationClip> mainHeroClips = {
      {0, {"assets/npc/main_idle.png", 12, 0.15f, mainRect}},
      {1, {"assets/npc/main_walk.png", 6, 0.08f, mainRect}},
  };

  sf::Vector2f playerStartPos{width / 2.f, height / 2.f};
  auto main_hero = systems::createNPC(m_registry, playerStartPos, mainSize, mainHeroClips, 200.f);
  m_registry.emplace<engine::PlayerControlled>(main_hero);
  m_registry.emplace<engine::CastsShadow>(main_hero);
  HP hp{100, 100};
  m_registry.emplace<HP>(main_hero, hp);
  m_registry.emplace<HpRegen>(main_hero, HpRegen{0.f, 0.f});
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
      1,    // shots per attack
      0.1f, // shot interval
      5     // dmg
  );

  const unsigned magicBallTexSize = 32u;
  const unsigned swordRingTexSize = 64u;
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
  uiEntities.gameSpeed = m_registry.create();
  uiEntities.pause = entt::null;

  std::string hpText = "HP " + std::to_string(hp.current) + "/" + std::to_string(hp.max);
  uiAssets.hp = textToImage(hpText, uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::Red);
  std::string expText = "Level " + std::to_string(exp.level) + " " + std::to_string(exp.currentXp) + "/" +
                        std::to_string(exp.xpToNextLevel);
  uiAssets.exp = textToImage(expText, uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::Cyan);
  uiAssets.kills = textToImage("Kills 0", uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::White);
  uiAssets.timer = textToImage("00:00", uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::White);
  {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(gameSpeed));
    uiAssets.gameSpeed =
        textToImage(std::string("Game speed ") + buf + "x", uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::White);
  }

  uiAssets.gameOver =
      textToImage("You died\nPress Esc to quit", uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::White);

  bool pauseLoaded = uiAssets.pause.loadFromFile("assets/ui/pause.png");
  (void)pauseLoaded;

  bool upgradeLoaded = upgradeUi.panel.loadFromFile("assets/ui/upgrade.png");
  (void)upgradeLoaded;

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
  UISprite uiGameSpeed{};
  uiGameSpeed.image = &uiAssets.gameSpeed;
  uiGameSpeed.pos = engine::Position{sf::Vector2f{m_engine->camera.size.x - 280.f, 10.f}};

  m_registry.emplace<UISprite>(uiEntities.hp, uiHP);
  m_registry.emplace<UISprite>(uiEntities.exp, uiExp);
  m_registry.emplace<UISprite>(uiEntities.kills, uiKills);
  m_registry.emplace<UISprite>(uiEntities.timer, uiTimer);
  m_registry.emplace<UISprite>(uiEntities.gameSpeed, uiGameSpeed);
}

void GameLoop::update(engine::Input &input, float dt) {
  dt *= gameSpeed;

  globalTimer += dt;
  spawnTimer += dt;
  uiTimer += dt;

  engine::Camera &camera = m_engine->camera;
  auto playerView = m_registry.view<const engine::Position, Experience, engine::PlayerControlled>();

  // Game over handling: if player HP is zero, show message and wait for exit.
  auto playerHpView = m_registry.view<const HP, const engine::PlayerControlled>();
  if (playerHpView.begin() != playerHpView.end()) {
    const auto &playerHp = playerHpView.get<const HP>(*playerHpView.begin());
    if (playerHp.current == 0)
      gameOverActive = true;
  }

  if (gameOverActive) {
    gameSpeed = 0.f;
    updateGameOverOverlay();

    if (input.isKeyDown(sf::Keyboard::Key::Escape)) {
      exit();
      return;
    }

    updateHUD();
    return;
  }

  if (upgradeMenuActive) {
    if (input.isKeyDown(sf::Keyboard::Key::Num1)) {
      applyUpgrade(upgradeUi.optionKinds[0]);
      closeUpgradeMenu();
    } else if (input.isKeyDown(sf::Keyboard::Key::Num2)) {
      applyUpgrade(upgradeUi.optionKinds[1]);
      closeUpgradeMenu();
    } else if (input.isKeyDown(sf::Keyboard::Key::Num3)) {
      applyUpgrade(upgradeUi.optionKinds[2]);
      closeUpgradeMenu();
    }

    updateHUD();
  } else {
    spawnMinotaurs();
    gameInputSystem(m_registry, input, gameSpeed);
    gameNpcFollowPlayerSystem(m_registry, camera);
    gameWeaponSystem(m_registry, dt, m_engine->camera);
    gameMovementSystem(m_registry, tiles, width, height, dt, globalTimer, m_engine->camera);
    gameProjectileDamageSystem(m_registry, dt, m_engine->camera);
    gameAnimationSystem(m_registry, dt);
    updatePlayerDamageColor(m_registry, globalTimer);

    auto regenView = m_registry.view<HP, HpRegen>();
    for (auto e : regenView) {
      auto &hpRegen = regenView.get<HpRegen>(e);
      auto &hpComp = regenView.get<HP>(e);
      if (hpRegen.perSecond <= 0.f)
        continue;
      hpRegen.accumulator += hpRegen.perSecond * dt;
      while (hpRegen.accumulator >= 1.f && hpComp.current < hpComp.max) {
        hpComp.current += 1;
        hpRegen.accumulator -= 1.f;
      }
    }

    unsigned int killed = clearDeadNpc(m_registry);
    kills += killed;
    Experience &exp = playerView.get<Experience>(*(playerView.begin()));
    const unsigned int expPerKill = 10;
    float xpGain = static_cast<float>(killed * expPerKill) * xpMultiplier;
    exp.currentXp += static_cast<unsigned int>(xpGain);
    unsigned int levelUps = 0;
    while (exp.currentXp >= exp.xpToNextLevel) {
      exp.currentXp -= exp.xpToNextLevel;
      exp.level += 1;
      exp.xpToNextLevel = static_cast<unsigned int>(exp.xpToNextLevel * 1.1f);
      ++levelUps;
    }
    pendingLevelUps += levelUps;
    if (pendingLevelUps > 0 && !upgradeMenuActive) {
      openUpgradeMenu();
    }

    updateUI();
  }

  // Move camera
  const auto &pos = playerView.get<const engine::Position>(*(playerView.begin()));
  m_engine->camera.position = m_engine->camera.worldToScreen(pos.value);
}

void GameLoop::collectRenderData(engine::RenderFrame &frame, engine::Camera &camera) {
  m_engine->render.renderMap(m_tileMeshes, camera, sf::Vector2i({width, height}), frame.tileBatches);
  systems::renderSystem(m_registry, frame, camera, m_engine->imageManager);
  uiRender(m_registry, frame, camera);
}

bool GameLoop::isFinished() const { return m_finished; }

sf::Image GameLoop::timerImage() {
  int time = static_cast<int>(globalTimer);
  int minutes = time / 60;
  int seconds = time % 60;
  char text[6];
  std::snprintf(text, sizeof(text), "%02d:%02d", minutes, seconds);
  return textToImage(std::string(text), uiFont, TIMER_TEXT_SIZE, sf::Color::White);
}

void GameLoop::updateUI() {
  updatePauseOverlay();
  updateStatsPanel();
  updateHUD();
}

void GameLoop::updateGameOverOverlay() {
  if (uiEntities.gameOver != entt::null && m_registry.valid(uiEntities.gameOver)) {
    return;
  }

  auto e = m_registry.create();
  UISprite sprite{};
  sprite.image = &uiAssets.gameOver;

  float w = static_cast<float>(uiAssets.gameOver.getSize().x);
  float h = static_cast<float>(uiAssets.gameOver.getSize().y);
  sprite.pos = engine::Position{
      {m_engine->camera.size.x * 0.5f - w * 0.5f, m_engine->camera.size.y * 0.5f - h * 0.5f}};
  sprite.zIndex = 10;

  m_registry.emplace<UISprite>(e, sprite);
  m_registry.emplace<UIGameOver>(e);
  uiEntities.gameOver = e;
}

void GameLoop::updateHUD() {
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
  {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(gameSpeed));
    uiAssets.gameSpeed =
        textToImage(std::string("Game speed ") + buf + "x", uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::White);
  }

  uiTimer = 0.0;
  return;
}

void GameLoop::updatePauseOverlay() {
  if (upgradeMenuActive) {
    if (uiEntities.pause != entt::null && m_registry.valid(uiEntities.pause)) {
      m_registry.destroy(uiEntities.pause);
      uiEntities.pause = entt::null;
    }
    return;
  }

  bool hasPause = uiEntities.pause != entt::null && m_registry.valid(uiEntities.pause) &&
                  m_registry.all_of<UISprite, UIPause>(uiEntities.pause);

  if (gameSpeed == 0.0f) {
    if (!hasPause && uiAssets.pause.getSize().x > 0 && uiAssets.pause.getSize().y > 0) {
      auto e = m_registry.create();
      UISprite sprite{};
      sprite.image = &uiAssets.pause;

      float w = static_cast<float>(uiAssets.pause.getSize().x);
      float h = static_cast<float>(uiAssets.pause.getSize().y);
      sprite.pos = engine::Position{
          {m_engine->camera.size.x * 0.5f - w * 0.5f, m_engine->camera.size.y * 0.5f - h * 0.5f}};
      sprite.zIndex = 2;

      m_registry.emplace<UISprite>(e, sprite);
      m_registry.emplace<UIPause>(e);
      uiEntities.pause = e;
    }
  } else {
    if (hasPause) {
      m_registry.destroy(uiEntities.pause);
      uiEntities.pause = entt::null;
    }
  }
}

void GameLoop::updateStatsPanel() {
  // Stats visible only together with pause overlay (ESC pause, no upgrade menu).
  bool hasPause = uiEntities.pause != entt::null && m_registry.valid(uiEntities.pause) &&
                  m_registry.all_of<UISprite, UIPause>(uiEntities.pause);

  if (upgradeMenuActive || !hasPause) {
    if (uiEntities.stats != entt::null && m_registry.valid(uiEntities.stats)) {
      m_registry.destroy(uiEntities.stats);
      uiEntities.stats = entt::null;
    }
    return;
  }

  auto view = m_registry.view<const engine::PlayerControlled,
      const HP,
      const Experience,
      const Weapons,
      const engine::Speed,
      const HpRegen>();

  if (view.begin() == view.end())
    return;

  auto player = *view.begin();
  const auto &hp = view.get<const HP>(player);
  const auto &exp = view.get<const Experience>(player);
  const auto &weapons = view.get<const Weapons>(player);
  const auto &speed = view.get<const engine::Speed>(player);
  const auto &regen = view.get<const HpRegen>(player);

  const Weapon &w0 = weapons.slots[0];
  const Weapon &w1 = weapons.slots[1];

  char buf[512];
  std::snprintf(buf,
      sizeof(buf),
      "HP: %u / %u\n"
      "HP regen: %.1f / min\n"
      "Level: %u\n"
      "XP: %u / %u\n"
      "Move speed: %.0f\n"
      "Magic dmg %u, rad %.1f\n"
      "  cd %.2f, shots %u\n"
      "Sword dmg %u, rad %.1f\n"
      "  cd %.2f, shots %u\n"
      "XP bonus: x%.2f\n"
      "Enemy count: x%.2f",
      hp.current,
      hp.max,
      regen.perSecond * 60.f,
      exp.level,
      exp.currentXp,
      exp.xpToNextLevel,
      speed.value,
      w0.damage,
      w0.radius,
      w0.cooldown,
      w0.shotsPerAttack,
      w1.damage,
      w1.radius,
      w1.cooldown,
      w1.shotsPerAttack,
      xpMultiplier,
      mobSpawnMultiplier);

  uiAssets.stats = textToImage(buf, uiFont, 20, sf::Color::White);

  if (uiEntities.stats == entt::null || !m_registry.valid(uiEntities.stats)) {
    auto e = m_registry.create();
    UISprite sprite{};
    sprite.image = &uiAssets.stats;
    sprite.pos = engine::Position{
        sf::Vector2f{m_engine->camera.size.x - 280.f, m_engine->camera.size.y * 0.5f - 150.f}};
    // Draw stats above pause background.
    sprite.zIndex = 3;
    m_registry.emplace<UISprite>(e, sprite);
    uiEntities.stats = e;
  }
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
  count = static_cast<int>(static_cast<double>(count) * (usedTime / 2.0));
  count = std::max(1, count);
  count = static_cast<int>(std::max(1.0, static_cast<double>(count) * mobSpawnMultiplier));
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

void GameLoop::spawnStaticObjects(unsigned int count) {
  struct Prefab {
    const char *path;
    int weight;
  };

  static const std::vector<Prefab> prefabs = {
      // bushes – most common
      {"assets/worlds/bush1.png", 20},
      {"assets/worlds/bush2.png", 20},

      // trees
      {"assets/worlds/tree1.png", 10},
      {"assets/worlds/tree2.png", 10},
      {"assets/worlds/tree3.png", 2},
      {"assets/worlds/tree4.png", 2},

      // stumps (broken)
      {"assets/worlds/broken1.png", 1},
      {"assets/worlds/broken2.png", 1},
      {"assets/worlds/broken3.png", 2},
  };

  if (prefabs.empty() || width <= 0 || height <= 0 || count == 0u)
    return;

  static std::mt19937 rng{std::random_device{}()};
  int totalWeight = 0;
  for (const auto &p : prefabs)
    totalWeight += p.weight;
  if (totalWeight <= 0)
    return;

  std::uniform_int_distribution<int> weightDist(0, totalWeight - 1);

  const float margin = 1.f;

  for (unsigned int i = 0; i < count; ++i) {
    sf::Vector2f worldPos = randomPointOnMap(width, height, margin);

    int w = weightDist(rng);
    const char *texPath = nullptr;
    for (const auto &p : prefabs) {
      if (w < p.weight) {
        texPath = p.path;
        break;
      }
      w -= p.weight;
    }
    if (!texPath)
      continue;

    sf::Image &img = m_engine->imageManager.getImage(texPath);
    sf::Vector2u texSize = img.getSize();
    if (texSize.x == 0u || texSize.y == 0u)
      continue;

    sf::IntRect rect({0, 0}, {static_cast<int>(texSize.x), static_cast<int>(texSize.y)});
    sf::Vector2f targetSize{static_cast<float>(texSize.x), static_cast<float>(texSize.y)};

    auto e = systems::createStaticObject(m_registry, worldPos, targetSize, texPath, rect);
    m_registry.emplace<engine::CastsShadow>(e);
  }
}

void GameLoop::openUpgradeMenu() {
  upgradeMenuActive = true;
  gameSpeed = 0.f;

  static std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<std::size_t> dist(0, ALL_UPGRADES.size() - 1);

  std::vector<std::size_t> indices;
  indices.reserve(3);
  while (indices.size() < 3 && indices.size() < ALL_UPGRADES.size()) {
    std::size_t idx = dist(rng);
    if (std::find(indices.begin(), indices.end(), idx) == indices.end())
      indices.push_back(idx);
  }

  for (int i = 0; i < 3; ++i) {
    std::size_t idx = indices[i % indices.size()];
    const auto &def = ALL_UPGRADES[idx];
    upgradeUi.optionKinds[i] = def.kind;
    upgradeUi.options[i] = textToImage(def.description, uiFont, DEFAULT_UI_TEXT_SIZE, sf::Color::White);
  }

  float panelWidth = static_cast<float>(upgradeUi.panel.getSize().x);
  float panelHeight = static_cast<float>(upgradeUi.panel.getSize().y);
  float panelX = m_engine->camera.size.x * 0.5f - panelWidth * 0.5f;
  float panelY = m_engine->camera.size.y * 0.5f - panelHeight * 0.5f;

  if (upgradeUi.panelEntity == entt::null || !m_registry.valid(upgradeUi.panelEntity)) {
    auto e = m_registry.create();
    UISprite sprite{};
    sprite.image = &upgradeUi.panel;
    sprite.pos = engine::Position{{panelX, panelY}};
    sprite.zIndex = 3;
    m_registry.emplace<UISprite>(e, sprite);
    upgradeUi.panelEntity = e;
  }

  for (int i = 0; i < 3; ++i) {
    if (upgradeUi.optionEntities[i] != entt::null && m_registry.valid(upgradeUi.optionEntities[i])) {
      m_registry.destroy(upgradeUi.optionEntities[i]);
      upgradeUi.optionEntities[i] = entt::null;
    }

    auto e = m_registry.create();
    UISprite sprite{};
    sprite.image = &upgradeUi.options[i];

    // Fixed offsets inside upgrade panel (same X, stepped Y).
    float baseX = panelX + 420.f;
    float baseY = panelY + 300.f;
    float stepY = 135.f;
    sprite.pos = engine::Position{{baseX, baseY + i * stepY}};
    sprite.zIndex = 4;

    m_registry.emplace<UISprite>(e, sprite);
    upgradeUi.optionEntities[i] = e;
  }
}

void GameLoop::closeUpgradeMenu() {
  upgradeMenuActive = false;
  gameSpeed = 1.0f;

  if (upgradeUi.panelEntity != entt::null && m_registry.valid(upgradeUi.panelEntity)) {
    m_registry.destroy(upgradeUi.panelEntity);
    upgradeUi.panelEntity = entt::null;
  }

  for (int i = 0; i < 3; ++i) {
    if (upgradeUi.optionEntities[i] != entt::null && m_registry.valid(upgradeUi.optionEntities[i])) {
      m_registry.destroy(upgradeUi.optionEntities[i]);
    }
    upgradeUi.optionEntities[i] = entt::null;
  }

  if (pendingLevelUps > 0) {
    --pendingLevelUps;
    if (pendingLevelUps > 0) {
      openUpgradeMenu();
    }
  }
}

void GameLoop::applyUpgrade(UpgradeKind kind) {
  auto view =
      m_registry.view<HP, Experience, Weapons, engine::Speed, HpRegen, const engine::PlayerControlled>();

  if (view.begin() == view.end())
    return;

  auto player = *view.begin();
  auto &hp = view.get<HP>(player);
  auto &exp = view.get<Experience>(player);
  auto &weapons = view.get<Weapons>(player);
  auto &speed = view.get<engine::Speed>(player);
  auto &regen = view.get<HpRegen>(player);

  (void)exp;

  switch (kind) {
  case UpgradeKind::MoveSpeed:
    speed.value += 50.f;
    break;
  case UpgradeKind::ExtraProjectiles:
    for (auto &w : weapons.slots)
      w.shotsPerAttack += 1;
    break;
  case UpgradeKind::Damage:
    for (auto &w : weapons.slots)
      w.damage += 5;
    break;
  case UpgradeKind::Radius: {
    float deltaRadius = 100.f / 64.f;
    for (auto &w : weapons.slots)
      w.radius += deltaRadius;
  } break;
  case UpgradeKind::Cooldown:
    for (auto &w : weapons.slots) {
      w.cooldown *= 0.9f;
      w.shotInterval *= 0.9f;
    }
    break;
  case UpgradeKind::MaxHp:
    hp.max += 30;
    hp.current += 30;
    break;
  case UpgradeKind::Regen:
    regen.perSecond += 40.f / 60.f;
    break;
  case UpgradeKind::XpGain:
    xpMultiplier *= 1.2f;
    break;
  case UpgradeKind::MobCount:
    mobSpawnMultiplier *= 1.1f;
    break;
  }

  uiTimer = 0.3;
  updateHUD();
}
