#pragma once

#include "core/loop.h"
#include "ecs/tile.h"
#include "resources/serializable_world.h"
#include <SFML/Graphics/VertexArray.hpp>
#include <entt/entt.hpp>
#include <vector>

namespace engine {
struct Engine;
struct Camera;
struct RenderFrame;
} // namespace engine

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
	void collectRenderData(engine::RenderFrame &frame,
						   engine::Camera &camera) override;

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

	engine::Engine *m_engine = nullptr; ///< Pointer to the main engine instance
	bool m_playerDied = false; ///< Flag tracking if player has died
	float m_gameOverTimer = 0.f; ///< Timer for game over screen display
	bool m_playerWon = false; ///< Flag tracking if player has won
	float m_winTimer = 0.f; ///< Timer for win screen display
	
	/**
	 * @brief Weapon types configuration.
	 */
	struct WeaponType {
		float fireRate;
		float bulletSpeed;
		float damage;
		std::string textureName;
		std::string shootTextureName;
	};
	
	std::vector<WeaponType> m_weaponTypes; ///< Available weapon types
	int m_currentWeaponIndex = 0; ///< Current weapon index (0-2)
	entt::entity m_playerEntity = entt::null; ///< Player entity reference

	/**
	 * @brief Updates animation states based on entity movement.
	 * @param dt Delta time in seconds for animation timing.
	 *
	 * Transitions between idle and run animations depending on entity velocity.
	 * Manages animation state changes and frame resetting.
	 */
	void gameAnimationSystem(float dt);

	/**
	 * @brief Renders the game over UI overlay and message.
	 * @param frame Reference to the render frame for collecting draw commands.
	 * @param camera Reference to the camera for positioning.
	 *
	 * Draws a semi-transparent overlay and "GAME OVER" message when player dies.
	 */
	void renderGameOverScreen(engine::RenderFrame &frame, engine::Camera &camera);

	/**
	 * @brief Renders the win UI overlay and message.
	 * @param frame Reference to the render frame for collecting draw commands.
	 * @param camera Reference to the camera for positioning.
	 *
	 * Draws a semi-transparent overlay and "YOU WIN" message when player wins.
	 */
	void renderWinScreen(engine::RenderFrame &frame, engine::Camera &camera);

	/**
	 * @brief Checks if all enemies are dead and player has won.
	 * @return True if player has won (all enemies dead), false otherwise.
	 */
	bool checkPlayerWin() const;

	/**
	 * @brief Handles weapon switching for the player.
	 * @param input Reference to the input system for reading user commands.
	 */
	void handleWeaponSwitching(const engine::Input &input);

	/**
	 * @brief Applies weapon configuration to player entity.
	 * @param playerEntity Player entity to apply weapon to.
	 * @param weaponIndex Index of weapon type to apply (0-2).
	 */
	void applyWeaponToPlayer(entt::entity playerEntity, int weaponIndex);

	int width;	///< World width in tile units
	int height; ///< World height in tile units
	std::unordered_map<int, engine::TileTexture>
		tileTextures;				   ///< Tile ID to texture data mapping
	sf::VertexArray m_staticMapPoints; ///< Pre-computed vertex data for static
									   ///< ground layer rendering
	std::vector<engine::Tile>
		tiles; ///< Tile data representing world layout, collision, and layers
};
