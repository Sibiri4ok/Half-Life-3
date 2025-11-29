#include "game_loop.h"

#include "core/camera.h"
#include "core/engine.h"
#include "core/render.h"
#include "core/render_frame.h"
#include "ecs/components.h"
#include "ecs/systems.h"
#include "ecs/utils.h"
#include "ecs/world_loader.h"
#include "loops/menu_loop.h"
#include "resources/image_manager.h"
#include <memory>
#include <random>

GameLoop::GameLoop() {
	engine::WorldLoader::loadWorldFromJson("game/assets/worlds/meadow.json", width,
										   height, tileTextures, tiles);
}

void GameLoop::init() {
	m_engine = engine::Engine::get();
	sf::Vector2f worldCenter = {width / 2.0f, height / 2.0f};
	sf::Vector2f screenCenter = m_engine->camera.worldToScreen(worldCenter);
	m_engine->camera.position = screenCenter;

	// Create and generate world tiles once

	const int tileWidth = 32.f;
	const int tileHeight = 32.f;
	m_engine->camera.setTileSize(tileWidth, tileHeight / 2);

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
						m_registry, worldPos, {32.f, 32.f}, texInfo.texture_src,
						sf::IntRect({0, 0}, {32, 32}));
					m_registry.emplace<engine::CastsShadow>(stObject);
				}
			}
			staticTiles[y * width + x].layerIds = std::move(groundLayers);
		}
	}

	m_engine->render.generateTileMapVertices(m_staticMapPoints, m_engine->camera,
											 staticTiles, width, height, tileImages);

	// Create entities (player, NPC, etc.)

	sf::Vector2f targetWolfSize{64.f, 64.f};
	sf::IntRect frameRect({0, 0}, {64, 64});

	// Wolf
	std::unordered_map<int, engine::AnimationClip> wolfClips = {
		{0, {"game/assets/critters/wolf/wolf-idle.png", 4, 0.15f, frameRect}},
		{1, {"game/assets/critters/wolf/wolf-run.png", 8, 0.08f, frameRect}},
	};

	// Initialize weapon types
	// Weapon 1: Standard (fireRate 0.3f, damage 10)
	// Weapon 2: Fast (fireRate 0.15f = 2x faster, damage 5)
	// Weapon 3: Heavy (fireRate 0.6f = 0.5x slower, damage 20)
	m_weaponTypes = {
		{0.3f, 20.f, 10.f, "game/assets/weapons/pistol-idle.png", "game/assets/weapons/pistol-shoot.png"},      // Standard
		{0.15f, 20.f, 5.f, "game/assets/weapons/machinegun-idle.png", "game/assets/weapons/machinegun-shoot.png"},  // Fast
		{0.6f, 20.f, 20.f, "game/assets/weapons/sniper-idle.png", "game/assets/weapons/sniper-shoot.png"}       // Heavy
	};
	m_currentWeaponIndex = 0;

	// Player wolf
	auto wolf =
		systems::createNPC(m_registry, {5.f, 5.f}, targetWolfSize, wolfClips, 5.f);
	m_registry.emplace<engine::PlayerControlled>(wolf);
	m_registry.emplace<engine::CastsShadow>(wolf);
	m_playerEntity = wolf;
	
	// Add health to player
	engine::Health playerHealth;
	playerHealth.current = 100.f;
	playerHealth.maximum = 100.f;
	m_registry.emplace<engine::Health>(wolf, playerHealth);
	
	// Add initial weapon to player
	applyWeaponToPlayer(wolf, 0);

	// Enemy wolf 1 (chaser)
	auto wolf1 =
		systems::createNPC(m_registry, {8.f, 8.f}, targetWolfSize, wolfClips, 2.5f);
	m_registry.emplace<engine::ChasingPlayer>(wolf1);
	m_registry.emplace<engine::CastsShadow>(wolf1);
	
	engine::Health enemy1Health;
	enemy1Health.current = 100.f;
	enemy1Health.maximum = 100.f;
	m_registry.emplace<engine::Health>(wolf1, enemy1Health);
	
	engine::Weapon enemy1Weapon;
	enemy1Weapon.fireRate = 0.4f;
	enemy1Weapon.bulletSpeed = 18.f;
	m_registry.emplace<engine::Weapon>(wolf1, enemy1Weapon);
	
	engine::WeaponDisplay enemy1WeaponDisplay;
	enemy1WeaponDisplay.textureName = "game/assets/weapons/pistol-idle.png";
	enemy1WeaponDisplay.size = {24.f, 24.f};
	m_registry.emplace<engine::WeaponDisplay>(wolf1, enemy1WeaponDisplay);
	
	engine::AICombat enemy1AI;
	enemy1AI.shootInterval = 1.2f;
	enemy1AI.detectionRange = 12.f;
	enemy1AI.shootingRange = 9.f;
	m_registry.emplace<engine::AICombat>(wolf1, enemy1AI);

	// Wandering wolves (enemies)
	for (int i = 0; i < 2; i++) {
		auto npc = systems::createNPC(m_registry, {i + 10.f, 0.f}, targetWolfSize,
									  wolfClips, 1.f);
		m_registry.emplace<engine::CastsShadow>(npc);
		
		engine::Health npcHealth;
		npcHealth.current = 100.f;
		npcHealth.maximum = 100.f;
		m_registry.emplace<engine::Health>(npc, npcHealth);
		
		engine::Weapon npcWeapon;
		npcWeapon.fireRate = 0.5f;
		npcWeapon.bulletSpeed = 16.f;
		m_registry.emplace<engine::Weapon>(npc, npcWeapon);
		
		engine::WeaponDisplay npcWeaponDisplay;
		npcWeaponDisplay.textureName = "game/assets/weapons/pistol-idle.png";
		npcWeaponDisplay.size = {24.f, 24.f};
		m_registry.emplace<engine::WeaponDisplay>(npc, npcWeaponDisplay);
		
		engine::AICombat npcAI;
		npcAI.shootInterval = 1.5f + i * 0.3f;
		npcAI.detectionRange = 10.f;
		npcAI.shootingRange = 8.f;
		m_registry.emplace<engine::AICombat>(npc, npcAI);
	}
}

void GameLoop::gameAnimationSystem(float dt) {
	auto view =
		m_registry.view<engine::Animation, engine::Velocity, engine::Renderable>();

	for (auto entity : view) {
		auto &anim = view.get<engine::Animation>(entity);
		auto &vel = view.get<engine::Velocity>(entity);
		auto &render = view.get<engine::Renderable>(entity);

		int newState =
			(std::sqrt(vel.value.x * vel.value.x + vel.value.y * vel.value.y) > 0.1f)
				? 1
				: 0;

		if (anim.clips.find(newState) != anim.clips.end() &&
			anim.state != newState) {
			anim.state = newState;
			anim.frameIdx = 0;
			anim.frameTime = 0.f;
		}
	}
}

void GameLoop::update(engine::Input &input, float dt) {
	// Check if player has died
	auto playerView = m_registry.view<const engine::PlayerControlled, const engine::Health>();
	for (auto entity : playerView) {
		const auto &health = playerView.get<const engine::Health>(entity);
		if (health.isDead || m_registry.all_of<engine::Dead>(entity)) {
			m_playerDied = true;
			m_gameOverTimer += dt;
			
			// Return to menu after 4 seconds of game over screen
			if (m_gameOverTimer >= 4.0f) {
				// Switch back to menu loop so player can restart
				auto menuLoop = std::make_unique<MenuLoop>();
				m_engine->setLoop(std::move(menuLoop));
				m_finished = true;
			}
			
			// Don't process game logic if player is dead
			return;
		}
	}
	
	// Check if player has won (all enemies are dead)
	if (!m_playerWon && checkPlayerWin()) {
		m_playerWon = true;
		m_winTimer = 0.f;
	}
	
	// Handle win screen
	if (m_playerWon) {
		m_winTimer += dt;
		
		// Return to menu after 4 seconds of win screen
		if (m_winTimer >= 4.0f) {
			// Switch back to menu loop so player can restart
			auto menuLoop = std::make_unique<MenuLoop>();
			m_engine->setLoop(std::move(menuLoop));
			m_finished = true;
		}
		
		// Don't process game logic if player has won
		return;
	}
	
	// Input and AI
	systems::playerInputSystem(m_registry, input);
	handleWeaponSwitching(input);
	systems::aiCombatSystem(m_registry, input, dt);
	
	// Combat
	systems::weaponSystem(m_registry, input, dt);
	systems::projectileSystem(m_registry, tiles, width, height, dt);
	systems::damageSystem(m_registry);
	systems::damageNumberSystem(m_registry, dt);
	systems::deathSystem(m_registry);
	
	// Movement
	systems::npcFollowPlayerSystem(m_registry, dt);
	systems::npcWanderSystem(m_registry, dt);
	systems::movementSystem(m_registry, tiles, width, height, dt);
	
	// Animation
	systems::animationSystem(m_registry, dt);
	gameAnimationSystem(dt);

	// Camera follow
	auto playerPosView =
		m_registry.view<const engine::Position, const engine::PlayerControlled>();
	for (auto entity : playerPosView) {
		const auto &pos = playerPosView.get<const engine::Position>(entity);
		m_engine->camera.position = m_engine->camera.worldToScreen(pos.value);
	}
}

void GameLoop::collectRenderData(engine::RenderFrame &frame,
								 engine::Camera &camera) {
	// Initialize vertex arrays
	frame.healthBarVertices.setPrimitiveType(sf::PrimitiveType::Points);
	frame.healthBarVertices.clear();
	
	// Collecting static map texture
	frame.tileVertices = m_staticMapPoints;

	// Collecting entities
	systems::renderSystem(m_registry, frame, camera, m_engine->imageManager);
	
	// Render weapons on top of entities
	systems::weaponDisplaySystem(m_registry, frame, camera, m_engine->imageManager);
	
	// Render health bars on top of everything
	systems::healthBarSystem(m_registry, frame, camera);
	
	// Render damage numbers (floating above entities)
	systems::damageNumberRenderSystem(m_registry, frame, camera);
	
	// Render win screen if player won
	if (m_playerWon) {
		renderWinScreen(frame, camera);
	}
	// Render game over screen if player died
	else if (m_playerDied) {
		renderGameOverScreen(frame, camera);
	}
}

void GameLoop::renderGameOverScreen(engine::RenderFrame &frame, engine::Camera &camera) {
	// Initialize vertex arrays for UI
	frame.uiOverlayVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
	frame.uiOverlayVertices.clear();
	frame.uiTextVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
	frame.uiTextVertices.clear();
	
	// Create semi-transparent dark overlay
	sf::Color overlayColor(0, 0, 0, 180); // Dark overlay with alpha
	sf::Vector2f topLeft(camera.position.x - camera.size.x / 2.f, 
						 camera.position.y - camera.size.y / 2.f);
	sf::Vector2f bottomRight(camera.position.x + camera.size.x / 2.f, 
							 camera.position.y + camera.size.y / 2.f);
	
	// Two triangles to form a rectangle overlay
	frame.uiOverlayVertices.append({topLeft, overlayColor});
	frame.uiOverlayVertices.append({{bottomRight.x, topLeft.y}, overlayColor});
	frame.uiOverlayVertices.append({bottomRight, overlayColor});
	
	frame.uiOverlayVertices.append({topLeft, overlayColor});
	frame.uiOverlayVertices.append({bottomRight, overlayColor});
	frame.uiOverlayVertices.append({{topLeft.x, bottomRight.y}, overlayColor});
	
	// Simple bitmap font for "GAME OVER" text
	// Each letter is defined as a 5x7 grid of pixels
	auto drawPixelChar = [&](int charData[7], float x, float y, float pixelSize, sf::Color color) {
		for (int row = 0; row < 7; ++row) {
			for (int col = 0; col < 5; ++col) {
				if (charData[row] & (1 << (4 - col))) {
					float px = x + col * pixelSize;
					float py = y + row * pixelSize;
					
					// Create two triangles for each pixel (quad)
					// Triangle 1
					frame.uiTextVertices.append({{px, py}, color});
					frame.uiTextVertices.append({{px + pixelSize, py}, color});
					frame.uiTextVertices.append({{px + pixelSize, py + pixelSize}, color});
					
					// Triangle 2
					frame.uiTextVertices.append({{px, py}, color});
					frame.uiTextVertices.append({{px + pixelSize, py + pixelSize}, color});
					frame.uiTextVertices.append({{px, py + pixelSize}, color});
				}
			}
		}
	};
	
	// Simple 5x7 bitmap font data
	// G
	int letterG[7] = {
		0b01110,
		0b10001,
		0b10000,
		0b10011,
		0b10001,
		0b10001,
		0b01110
	};
	
	// A
	int letterA[7] = {
		0b01110,
		0b10001,
		0b10001,
		0b11111,
		0b10001,
		0b10001,
		0b10001
	};
	
	// M
	int letterM[7] = {
		0b10001,
		0b11011,
		0b10101,
		0b10101,
		0b10001,
		0b10001,
		0b10001
	};
	
	// E
	int letterE[7] = {
		0b11111,
		0b10000,
		0b10000,
		0b11110,
		0b10000,
		0b10000,
		0b11111
	};
	
	// O
	int letterO[7] = {
		0b01110,
		0b10001,
		0b10001,
		0b10001,
		0b10001,
		0b10001,
		0b01110
	};
	
	// V
	int letterV[7] = {
		0b10001,
		0b10001,
		0b10001,
		0b10001,
		0b10001,
		0b01010,
		0b00100
	};
	
	// R
	int letterR[7] = {
		0b11110,
		0b10001,
		0b10001,
		0b11110,
		0b10100,
		0b10010,
		0b10001
	};
	
	// Calculate center position for text
	float pixelSize = 4.0f;
	float charSpacing = 6.0f * pixelSize;
	float wordSpacing = 10.0f * pixelSize;
	
	// "GAME" = 4 letters, "OVER" = 4 letters, 1 space between words
	float totalWidth = 4 * charSpacing + wordSpacing + 4 * charSpacing;
	float startX = camera.position.x - totalWidth / 2.f;
	float startY = camera.position.y - 20.0f;
	
	sf::Color textColor(255, 50, 50); // Red color for "GAME OVER"
	
	// Draw "GAME"
	drawPixelChar(letterG, startX, startY, pixelSize, textColor);
	drawPixelChar(letterA, startX + charSpacing, startY, pixelSize, textColor);
	drawPixelChar(letterM, startX + charSpacing * 2, startY, pixelSize, textColor);
	drawPixelChar(letterE, startX + charSpacing * 3, startY, pixelSize, textColor);
	
	// Draw "OVER"
	float overX = startX + charSpacing * 4 + wordSpacing;
	drawPixelChar(letterO, overX, startY, pixelSize, textColor);
	drawPixelChar(letterV, overX + charSpacing, startY, pixelSize, textColor);
	drawPixelChar(letterE, overX + charSpacing * 2, startY, pixelSize, textColor);
	drawPixelChar(letterR, overX + charSpacing * 3, startY, pixelSize, textColor);
}

bool GameLoop::checkPlayerWin() const {
	// Check if player is alive
	bool playerAlive = false;
	auto playerView = m_registry.view<const engine::PlayerControlled, const engine::Health>();
	for (auto entity : playerView) {
		const auto &health = playerView.get<const engine::Health>(entity);
		if (!health.isDead && !m_registry.all_of<engine::Dead>(entity)) {
			playerAlive = true;
			break;
		}
	}
	
	if (!playerAlive) {
		return false; // Player is dead, can't win
	}
	
	// Check if there are any alive enemies (entities with Health but without PlayerControlled)
	auto allEntities = m_registry.view<const engine::Health>();
	for (auto entity : allEntities) {
		// Skip player
		if (m_registry.all_of<engine::PlayerControlled>(entity)) {
			continue;
		}
		
		// Check if this enemy is alive
		const auto &health = allEntities.get<const engine::Health>(entity);
		if (!health.isDead && !m_registry.all_of<engine::Dead>(entity)) {
			return false; // Found alive enemy
		}
	}
	
	// All enemies are dead, player wins!
	return true;
}

void GameLoop::renderWinScreen(engine::RenderFrame &frame, engine::Camera &camera) {
	// Initialize vertex arrays for UI
	frame.uiOverlayVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
	frame.uiOverlayVertices.clear();
	frame.uiTextVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
	frame.uiTextVertices.clear();
	
	// Create semi-transparent dark overlay
	sf::Color overlayColor(0, 0, 0, 180); // Dark overlay with alpha
	sf::Vector2f topLeft(camera.position.x - camera.size.x / 2.f, 
						 camera.position.y - camera.size.y / 2.f);
	sf::Vector2f bottomRight(camera.position.x + camera.size.x / 2.f, 
							 camera.position.y + camera.size.y / 2.f);
	
	// Two triangles to form a rectangle overlay
	frame.uiOverlayVertices.append({topLeft, overlayColor});
	frame.uiOverlayVertices.append({{bottomRight.x, topLeft.y}, overlayColor});
	frame.uiOverlayVertices.append({bottomRight, overlayColor});
	
	frame.uiOverlayVertices.append({topLeft, overlayColor});
	frame.uiOverlayVertices.append({bottomRight, overlayColor});
	frame.uiOverlayVertices.append({{topLeft.x, bottomRight.y}, overlayColor});
	
	// Helper function to draw a pixel character
	auto drawPixelChar = [&](int charData[7], float x, float y, float pixelSize, sf::Color color) {
		for (int row = 0; row < 7; ++row) {
			for (int col = 0; col < 5; ++col) {
				if (charData[row] & (1 << (4 - col))) {
					float px = x + col * pixelSize;
					float py = y + row * pixelSize;
					
					// Create two triangles for each pixel (quad)
					frame.uiTextVertices.append({{px, py}, color});
					frame.uiTextVertices.append({{px + pixelSize, py}, color});
					frame.uiTextVertices.append({{px + pixelSize, py + pixelSize}, color});
					
					frame.uiTextVertices.append({{px, py}, color});
					frame.uiTextVertices.append({{px + pixelSize, py + pixelSize}, color});
					frame.uiTextVertices.append({{px, py + pixelSize}, color});
				}
			}
		}
	};
	
	// Simple 5x7 bitmap font data
	// Y
	int letterY[7] = {
		0b10001,
		0b10001,
		0b10001,
		0b01010,
		0b00100,
		0b00100,
		0b00100
	};
	
	// O
	int letterO[7] = {
		0b01110,
		0b10001,
		0b10001,
		0b10001,
		0b10001,
		0b10001,
		0b01110
	};
	
	// U
	int letterU[7] = {
		0b10001,
		0b10001,
		0b10001,
		0b10001,
		0b10001,
		0b10001,
		0b01110
	};
	
	// W
	int letterW[7] = {
		0b10001,
		0b10001,
		0b10001,
		0b10101,
		0b10101,
		0b11011,
		0b10001
	};
	
	// I
	int letterI[7] = {
		0b11111,
		0b00100,
		0b00100,
		0b00100,
		0b00100,
		0b00100,
		0b11111
	};
	
	// N
	int letterN[7] = {
		0b10001,
		0b10001,
		0b11001,
		0b10101,
		0b10011,
		0b10001,
		0b10001
	};
	
	// Calculate center position for text
	float pixelSize = 4.0f;
	float charSpacing = 6.0f * pixelSize;
	float wordSpacing = 10.0f * pixelSize;
	
	// "YOU" = 3 letters, "WIN" = 3 letters, 1 space between words
	float totalWidth = 3 * charSpacing + wordSpacing + 3 * charSpacing;
	float startX = camera.position.x - totalWidth / 2.f;
	float startY = camera.position.y - 20.0f;
	
	sf::Color textColor(50, 255, 50); // Green color for "YOU WIN"
	
	// Draw "YOU"
	drawPixelChar(letterY, startX, startY, pixelSize, textColor);
	drawPixelChar(letterO, startX + charSpacing, startY, pixelSize, textColor);
	drawPixelChar(letterU, startX + charSpacing * 2, startY, pixelSize, textColor);
	
	// Draw "WIN"
	float winX = startX + charSpacing * 3 + wordSpacing;
	drawPixelChar(letterW, winX, startY, pixelSize, textColor);
	drawPixelChar(letterI, winX + charSpacing, startY, pixelSize, textColor);
	drawPixelChar(letterN, winX + charSpacing * 2, startY, pixelSize, textColor);
}

void GameLoop::handleWeaponSwitching(const engine::Input &input) {
	if (m_playerEntity == entt::null || !m_registry.valid(m_playerEntity)) {
		return;
	}

	// Check for weapon switch keys (1, 2, 3) - only switch once per key press
	// We need to track previous key state to detect key press events
	static bool prevKey1 = false, prevKey2 = false, prevKey3 = false;
	
	bool key1 = input.isKeyDown(sf::Keyboard::Key::Num1) || input.isKeyDown(sf::Keyboard::Key::Numpad1);
	bool key2 = input.isKeyDown(sf::Keyboard::Key::Num2) || input.isKeyDown(sf::Keyboard::Key::Numpad2);
	bool key3 = input.isKeyDown(sf::Keyboard::Key::Num3) || input.isKeyDown(sf::Keyboard::Key::Numpad3);
	
	int newWeaponIndex = -1;
	if (key1 && !prevKey1) {
		newWeaponIndex = 0;
	} else if (key2 && !prevKey2) {
		newWeaponIndex = 1;
	} else if (key3 && !prevKey3) {
		newWeaponIndex = 2;
	}
	
	prevKey1 = key1;
	prevKey2 = key2;
	prevKey3 = key3;

	if (newWeaponIndex >= 0 && newWeaponIndex < static_cast<int>(m_weaponTypes.size()) && 
		newWeaponIndex != m_currentWeaponIndex) {
		applyWeaponToPlayer(m_playerEntity, newWeaponIndex);
		m_currentWeaponIndex = newWeaponIndex;
	}
}

void GameLoop::applyWeaponToPlayer(entt::entity playerEntity, int weaponIndex) {
	if (weaponIndex < 0 || weaponIndex >= static_cast<int>(m_weaponTypes.size())) {
		return;
	}

	const WeaponType &weaponType = m_weaponTypes[weaponIndex];

	// Update or create Weapon component
	if (m_registry.all_of<engine::Weapon>(playerEntity)) {
		auto &weapon = m_registry.get<engine::Weapon>(playerEntity);
		weapon.fireRate = weaponType.fireRate;
		weapon.bulletSpeed = weaponType.bulletSpeed;
		weapon.damage = weaponType.damage;
		// Don't reset timeSinceLastShot to allow smooth switching
	} else {
		engine::Weapon weapon;
		weapon.fireRate = weaponType.fireRate;
		weapon.bulletSpeed = weaponType.bulletSpeed;
		weapon.damage = weaponType.damage;
		m_registry.emplace<engine::Weapon>(playerEntity, weapon);
	}

	// Update or create WeaponDisplay component
	if (m_registry.all_of<engine::WeaponDisplay>(playerEntity)) {
		auto &weaponDisplay = m_registry.get<engine::WeaponDisplay>(playerEntity);
		weaponDisplay.textureName = weaponType.textureName;
		weaponDisplay.shootTextureName = weaponType.shootTextureName;
	} else {
		engine::WeaponDisplay weaponDisplay;
		weaponDisplay.textureName = weaponType.textureName;
		weaponDisplay.shootTextureName = weaponType.shootTextureName;
		weaponDisplay.offset = {0.3f, -0.1f};
		weaponDisplay.size = {24.f, 24.f};
		m_registry.emplace<engine::WeaponDisplay>(playerEntity, weaponDisplay);
	}
}

bool GameLoop::isFinished() const { return m_finished; }
