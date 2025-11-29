#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>

namespace engine {

/**
 * @brief Component representing 2D position in world space.
 */
struct Position {
	sf::Vector2f value;
};

/**
 * @brief Component representing movement speed scalar.
 */
struct Speed {
	float value;
};

/**
 * @brief Component representing 2D velocity vector.
 */
struct Velocity {
	sf::Vector2f value;
};

/**
 * @brief Component representing rotation angle in degrees.
 */
struct Rotation {
	float angle = 0.f;
};

/**
 * @brief Enumeration for cardinal directions used in animations.
 */
enum class Direction { Down = 0, Right = 1, Left = 2, Up = 3 };

/**
 * @brief Component defining an animation clip with timing and frame data.
 */
struct AnimationClip {
	std::string texture;		// Texture name
	int frameCount = 1;			// Number of frames per line
	float frameDuration = 0.1f; // Time of one frame
	sf::IntRect frameRect;		// Size of one frame
};

/**
 * @brief Component managing animation state and playback.
 */
struct Animation {
	std::unordered_map<int, AnimationClip> clips;
	int state = 0;						   // Current state
	int frameIdx = 0;					   // Current frame
	float frameTime = 0.f;				   // Accumulated time
	int row = 0;						   // Current line (direction)
	Direction direction = Direction::Down; // Current direction
};

/**
 * @brief Component defining visual representation of an entity.
 */
struct Renderable {
	std::string textureName; // Used to get texture from your Texture manager
	sf::IntRect textureRect; // Base rect for frame 0
	sf::Vector2f targetSize;
	sf::Color color = sf::Color::White;
};

/**
 * @brief Tag component indicating an entity should cast a shadow.
 */
struct CastsShadow {};

/**
 * @brief Tag component for NPC entities that chase the player.
 */
struct ChasingPlayer {};

/**
 * @brief Tag component identifying player-controlled entities.
 */
struct PlayerControlled {};

/**
 * @brief Component representing a weapon that can shoot projectiles.
 */
struct Weapon {
	float fireRate = 0.5f;		  // Time between shots in seconds
	float timeSinceLastShot = 0.f; // Accumulated time since last shot
	float bulletSpeed = 15.f;	  // Speed of bullets
	float damage = 10.f;			  // Damage dealt by bullets
};

/**
 * @brief Component representing a projectile (bullet).
 */
struct Projectile {
	sf::Vector2f direction; // Direction of movement
	float lifetime = 3.f;	// Time before auto-destruction
	float timeAlive = 0.f;	// Time since creation
};

/**
 * @brief Tag component indicating entity is currently shooting.
 */
struct IsShooting {
	float animationTime = 0.f; // Time for muzzle flash animation
};

/**
 * @brief Component for displaying a weapon attached to an entity.
 */
struct WeaponDisplay {
	std::string textureName = "game/assets/weapons/pistol-idle.png";
	std::string shootTextureName = "game/assets/weapons/pistol-shoot.png"; // Texture when shooting
	sf::Vector2f offset = {0.3f, 0.f}; // Offset from entity position
	sf::Vector2f size = {32.f, 32.f};
};

/**
 * @brief Component representing entity health.
 */
struct Health {
	float current = 100.f; // Current health points
	float maximum = 100.f; // Maximum health points
	bool isDead = false;   // Flag for death state
};

/**
 * @brief Component representing damage dealt by projectile.
 */
struct Damage {
	float amount = 10.f;					// Damage amount
	entt::entity owner = entt::null;		// Entity that fired this projectile
	bool hasOwner = false;					// Whether owner is set
};

/**
 * @brief Tag component indicating entity is dead.
 */
struct Dead {};

/**
 * @brief Component for displaying floating damage numbers.
 */
struct DamageNumber {
	float amount; ///< Damage amount to display
	float lifetime = 1.5f; ///< Time before disappearing
	float timeAlive = 0.f; ///< Time since creation
	sf::Vector2f position; ///< World position where damage occurred
};

/**
 * @brief Component for AI combat behavior.
 */
struct AICombat {
	float shootCooldown = 0.f;			// Time since last shot
	float shootInterval = 1.5f;			// Time between shots
	float detectionRange = 10.f;		// Range to detect enemies
	float shootingRange = 8.f;			// Range to start shooting
	entt::entity target = entt::null;	// Current target
	bool hasTarget = false;				// Whether target is set
};

} // namespace engine
