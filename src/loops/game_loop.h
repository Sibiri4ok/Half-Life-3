#pragma once

#include "core/loop.h"
#include "ecs/tile.h"
#include "resources/serializable_world.h"
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <entt/entt.hpp>
#include <vector>

namespace engine {
struct Engine;
struct Camera;
struct RenderFrame;
} // namespace engine

enum class UpgradeKind {
  MoveSpeed,
  ExtraProjectiles,
  Damage,
  Radius,
  Cooldown,
  MaxHp,
  Regen,
  XpGain,
  MobCount,
};

/**
 * @brief Main game loop implementation for gameplay scene.
 *
 * Handles game logic updates, entity management, and rendering data collection
 * for the primary gameplay state. Manages all game objects and tile-based world.
 */
class GameLoop : public engine::ILoop {
public:
  /**
   * @brief Constructs game loop and loads world data from JSON file.
   *
   * Initializes tile map and textures from serialized world data file.
   * Loads world layout, dimensions, and tile textures from JSON configuration.
   */
  GameLoop();
  virtual ~GameLoop() = default;

  /**
   * @brief Initializes the game state and creates all entities.
   *
   * Sets up the game world, creates static map geometry, spawns player character,
   * NPCs, and configures the camera. Loads textures and generates render data.
   */
  void init() override;

  /**
   * @brief Updates game logic for the current frame.
   * @param input Reference to the input system for player controls.
   * @param dt Delta time in seconds since last update.
   *
   * Processes player input, movement, animations, and camera tracking.
   * Executes all game systems in defined order.
   */
  void update(engine::Input &input, float dt) override;

  /**
   * @brief Collects all render data for the current frame.
   * @param frame Reference to the render frame to populate with draw commands.
   * @param camera Reference to the camera for view-relative rendering.
   *
   * Gathers static map geometry, entity sprites, and visual elements for
   * rendering. Combines pre-computed tile vertices with dynamic entity render
   * data.
   */
  void collectRenderData(engine::RenderFrame &frame, engine::Camera &camera) override;

  /**
   * @brief Checks if the game loop should terminate.
   * @return True if the game loop has finished execution, false otherwise.
   */
  bool isFinished() const override;

private:
  /**
   * @brief Container managing all game objects, their components and
   * relationships.
   *
   * The registry stores entities (game objects) and their components (data),
   * providing efficient querying and management of object relationships.
   * It serves as the database for all dynamic game elements.
   */
  entt::registry m_registry;

  double globalTimer = 0.0;
  double spawnTimer = 0.0;
  double uiTimer = 0.0;
  unsigned int kills = 0;

  float gameSpeed = 1.0;
  float xpMultiplier = 1.0f;
  float mobSpawnMultiplier = 1.0f;
  bool upgradeMenuActive = false;
  unsigned int pendingLevelUps = 0;
  bool gameOverActive = false;

  sf::Font uiFont;
  struct UiAssets {
    sf::Image hp;
    sf::Image exp;
    sf::Image kills;
    sf::Image timer;
    sf::Image gameSpeed;
    sf::Image pause;
    sf::Image stats;
    sf::Image gameOver;
  } uiAssets;
  struct UiEntities {
    entt::entity hp{entt::null};
    entt::entity exp{entt::null};
    entt::entity kills{entt::null};
    entt::entity timer{entt::null};
    entt::entity gameSpeed{entt::null};
    entt::entity pause{entt::null};
    entt::entity stats{entt::null};
    entt::entity gameOver{entt::null};
  } uiEntities;

  engine::Engine *m_engine = nullptr; ///< Pointer to the main engine instance

  int width;                                                 ///< World width in tile units
  int height;                                                ///< World height in tile units
  std::unordered_map<int, engine::TileTexture> tileTextures; ///< Tile ID to texture data mapping
  std::vector<sf::VertexArray> m_tileMeshes;                 ///< Cached meshes for tilemap layers
  std::vector<engine::Tile> tiles; ///< Tile data representing world layout, collision, and layers

  struct UpgradeUI {
    sf::Image panel;
    sf::Image options[3];
    entt::entity panelEntity{entt::null};
    entt::entity optionEntities[3]{entt::null, entt::null, entt::null};
    UpgradeKind optionKinds[3]{UpgradeKind::MoveSpeed, UpgradeKind::ExtraProjectiles, UpgradeKind::Damage};
  } upgradeUi;

  int getEmaFps();
  sf::Image timerImage();
  void updateUI();
  void updateHUD();
  void updatePauseOverlay();
  void updateStatsPanel();
  void updateGameOverOverlay();
  void openUpgradeMenu();
  void closeUpgradeMenu();
  void applyUpgrade(UpgradeKind kind);
  void spawnMinotaurs();
  void spawnStaticObjects(unsigned int count);
};
