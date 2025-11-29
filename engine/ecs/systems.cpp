#include "ecs/systems.h"

#include "core/camera.h"
#include "core/input.h"
#include "core/render_frame.h"
#include "ecs/components.h"
#include "ecs/utils.h"
#include "resources/image_manager.h"
#include <cmath>
#include <random>

#include <iostream>

namespace systems {

using namespace engine;

void playerInputSystem(entt::registry &registry, const Input &input) {
	auto view = registry.view<Velocity, PlayerControlled, Animation>();

	for (auto entity : view) {
		auto &vel = view.get<Velocity>(entity);
		auto &anim = view.get<Animation>(entity);

		vel.value = {0.f, 0.f};

		// movement in world coordinates
		if (input.isKeyDown(sf::Keyboard::Key::W)) {
			vel.value.y -= 1.f;
		}
		if (input.isKeyDown(sf::Keyboard::Key::S)) {
			vel.value.y += 1.f;
		}
		if (input.isKeyDown(sf::Keyboard::Key::A)) {
			vel.value.x -= 1.f;
		}
		if (input.isKeyDown(sf::Keyboard::Key::D)) {
			vel.value.x += 1.f;
		}

		float length =
			std::sqrt(vel.value.x * vel.value.x + vel.value.y * vel.value.y);
		if (length > 0.f) {
			vel.value /= length;

			if (std::abs(vel.value.x) > std::abs(vel.value.y)) {
				anim.row = (vel.value.x > 0.f) ? 1 : 2; // right=1, left=2
			} else {
				anim.row = (vel.value.y > 0.f) ? 0 : 3; // down=0, up=3
			}
		}
	}
}

void movementSystem(entt::registry &registry, std::vector<engine::Tile> &tiles,
					int worldWidth, int worldHeight, float dt) {
	auto view = registry.view<Position, const Velocity, const Speed>();
	auto getIndex = [&](int x, int y) { return y * worldWidth + x; };

	for (auto entity : view) {
		auto &pos = view.get<Position>(entity);
		const auto &vel = view.get<const Velocity>(entity);
		const auto &speed = view.get<const Speed>(entity);

		sf::Vector2f newPos = pos.value;
		sf::Vector2f delta = vel.value * speed.value * dt;

		auto canMove = [&](float newX, float newY) {
			int tileX = static_cast<int>(newX) - 1;
			int tileY = static_cast<int>(newY);
			if (tileX < 0 || tileX >= worldWidth || tileY < 0 ||
				tileY >= worldHeight)
				return false;
			return !tiles[getIndex(tileX, tileY)].solid;
		};

		if (canMove(pos.value.x + delta.x, pos.value.y))
			newPos.x += delta.x;
		if (canMove(newPos.x, pos.value.y + delta.y))
			newPos.y += delta.y;

		pos.value = newPos;
	}
}

void animationSystem(entt::registry &registry, float dt) {
	auto view = registry.view<Animation>();

	for (auto entity : view) {
		auto &anim = view.get<Animation>(entity);
		if (anim.clips.empty())
			continue;

		auto it = anim.clips.find(anim.state);
		if (it == anim.clips.end())
			continue;

		const auto &clip = it->second;
		if (clip.frameCount <= 1)
			continue;

		anim.frameTime += dt;
		while (anim.frameTime >= clip.frameDuration) {
			anim.frameTime -= clip.frameDuration;
			anim.frameIdx = (anim.frameIdx + 1) % clip.frameCount;
		}
	}
}

void renderSystem(entt::registry &registry, RenderFrame &frame, const Camera &camera,
				  ImageManager &imageManager) {
	sf::FloatRect boundsCamera = camera.getBounds();

	registry.sort<Position>([](const auto &lhs, const auto &rhs) {
		if (lhs.value.y != rhs.value.y) {
			return lhs.value.y < rhs.value.y;
		}
		return lhs.value.x < rhs.value.x;
	});

	auto view = registry.view<const Position, Renderable, const Velocity>();

	const sf::Vector2f shadowVector = {1.f, .0f};
	const sf::Color shadowColor(0, 0, 0, 100);
	const int shadowStep = 1;
	const int pointSize = static_cast<int>(std::ceil(camera.zoom));

	for (auto entity : view) {
		sf::VertexArray shadowVertices;
		shadowVertices.clear();
		shadowVertices.setPrimitiveType(sf::PrimitiveType::Points);
		const auto &pos = view.get<const Position>(entity);
		auto &render = view.get<Renderable>(entity);

		sf::FloatRect boundsEntity(camera.worldToScreen(pos.value),
								   {render.targetSize.x, render.targetSize.y});
		if (!boundsEntity.findIntersection(boundsCamera).has_value()) {
			continue;
		}

		const auto *anim = registry.try_get<const Animation>(entity);
		const auto *rot = registry.try_get<const Rotation>(entity);

		sf::IntRect currentFrameRect = render.textureRect;
		const sf::Image *entityImage = &imageManager.getImage(render.textureName);

		if (anim && !anim->clips.empty()) {
			auto it = anim->clips.find(anim->state);
			if (it != anim->clips.end()) {
				const auto &clip = it->second;
				entityImage = &imageManager.getImage(clip.texture);
				currentFrameRect.position.x +=
					currentFrameRect.size.x * anim->frameIdx;
				currentFrameRect.position.y += currentFrameRect.size.y * anim->row;
			}
		}

		// calculate content rect in case spritesheet with paddings.
		// TODO: precalculate
		sf::IntRect currentContentRect =
			engine::calculateContentRect(*entityImage, currentFrameRect);

		float frameWidth = static_cast<float>(currentFrameRect.size.x);
		float frameHeight = static_cast<float>(currentFrameRect.size.y);
		float contentWidth = static_cast<float>(currentContentRect.size.x);
		float contentHeight = static_cast<float>(currentContentRect.size.y);

		float uniformScale = camera.zoom;
		if (frameWidth > 0.f && frameHeight > 0.f) {
			float scaleX = render.targetSize.x / frameWidth;
			float scaleY = render.targetSize.y / frameHeight;
			uniformScale = std::min(scaleX, scaleY) * camera.zoom;
		}

		// float scaledFrameWidth = frameWidth * uniformScale;
		// float scaledFrameHeight = frameHeight * uniformScale;

		const sf::Vector2f anchor = camera.worldToScreen(pos.value);
		const float angle = (rot ? rot->angle * (3.14159f / 180.f) : 0.f);
		const float cosA = std::cos(angle);
		const float sinA = std::sin(angle);

		// generate shadow
		if (registry.all_of<CastsShadow>(entity)) {
			const sf::Image &img = *entityImage;

			int texW = currentContentRect.size.x;
			int texH = currentContentRect.size.y;
			int texLeft = currentContentRect.position.x;
			int texTop = currentContentRect.position.y;

			float contentAnchorX_tex =
				static_cast<float>(texLeft) + contentWidth * 0.5f;
			float contentAnchorY_tex = static_cast<float>(texTop) + contentHeight;

			for (int ty = 0; ty < texH; ty += shadowStep) {
				for (int tx = 0; tx < texW; tx += shadowStep) {

					int u = currentFrameRect.position.x + texLeft + tx;
					int v = currentFrameRect.position.y + texTop + ty;

					if (u < 0 || v < 0 || u >= static_cast<int>(img.getSize().x) ||
						v >= static_cast<int>(img.getSize().y))
						continue;
					if (img.getPixel({(unsigned int)u, (unsigned int)v}).a == 0)
						continue;

					float localX =
						(static_cast<float>(texLeft + tx) - contentAnchorX_tex) *
						uniformScale;
					float localY =
						(static_cast<float>(texTop + ty) - contentAnchorY_tex) *
						uniformScale;

					float rotatedX = localX * cosA - localY * sinA;
					float rotatedY = localX * sinA + localY * cosA;
					float z = -rotatedY;

					float shadowX = (anchor.x + rotatedX) + (z * shadowVector.x);
					float shadowY = (anchor.y + rotatedY) + (z * shadowVector.y);

					for (int dy = 0; dy < pointSize; ++dy) {
						for (int dx = 0; dx < pointSize; ++dx) {
							shadowVertices.append(
								{{shadowX + dx, shadowY + dy}, shadowColor});
						}
					}
				}
			}
		}

		// draw sprite's content at bottom middle
		sf::IntRect absoluteContentRect = currentContentRect;
		absoluteContentRect.position.x += currentFrameRect.position.x;
		absoluteContentRect.position.y += currentFrameRect.position.y;

		float contentWidth_scaled =
			static_cast<float>(currentContentRect.size.x) * uniformScale;
		float contentHeight_scaled =
			static_cast<float>(currentContentRect.size.y) * uniformScale;

		float localX_tl = -contentWidth_scaled * 0.5f;
		float localY_tl = -contentHeight_scaled;

		float rotatedX_tl = localX_tl * cosA - localY_tl * sinA;
		float rotatedY_tl = localX_tl * sinA + localY_tl * cosA;

		sf::Vector2f spriteDrawPos = anchor + sf::Vector2f(rotatedX_tl, rotatedY_tl);

		RenderFrame::SpriteData spriteData;
		spriteData.image = entityImage;
		spriteData.textureRect = absoluteContentRect;
		spriteData.scale = {uniformScale, uniformScale};
		spriteData.position = spriteDrawPos;
		spriteData.rotation = sf::degrees(angle / (3.14159f / 180.f));
		spriteData.color = render.color;
		spriteData.shadowVertices = shadowVertices;
		frame.sprites.push_back(spriteData);
	}
}

void npcFollowPlayerSystem(entt::registry &registry, float dt) {
	auto playerView = registry.view<const Position, const PlayerControlled>();

	const auto playerEntity = *playerView.begin();
	const auto &playerPos = playerView.get<const Position>(playerEntity);

	auto npcView =
		registry.view<const Position, Velocity, Animation, ChasingPlayer>();

	for (auto entity : npcView) {
		const auto &pos = npcView.get<Position>(entity);
		auto &vel = npcView.get<Velocity>(entity);
		auto &anim = npcView.get<Animation>(entity);

		sf::Vector2f dir = playerPos.value - pos.value;
		float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);

		if (len > 3.f) {
			vel.value = dir / len;
		} else {
			vel.value = {0.f, 0.f};
		}

		float length =
			std::sqrt(vel.value.x * vel.value.x + vel.value.y * vel.value.y);
		if (length > 0.01f) {
			sf::Vector2f dir = vel.value / length;

			if (std::abs(dir.x) > std::abs(dir.y)) {
				anim.row = (dir.x > 0.f) ? 1 : 2; // right = 1, left = 2
			} else {
				anim.row = (dir.y > 0.f) ? 0 : 3; // down = 0, up = 3
			}
		}
	}
}

void npcWanderSystem(entt::registry &registry, float dt) {
	auto view = registry.view<Position, Velocity, Animation>(
		entt::exclude<PlayerControlled, ChasingPlayer>);

	for (auto entity : view) {
		auto &vel = view.get<Velocity>(entity);
		auto &anim = view.get<Animation>(entity);

		float length =
			std::sqrt(vel.value.x * vel.value.x + vel.value.y * vel.value.y);
		if (length > 0.01f) {
			sf::Vector2f dir = vel.value / length;

			if (std::abs(dir.x) > std::abs(dir.y)) {
				anim.row = (dir.x > 0.f) ? 1 : 2; // right = 1, left = 2
			} else {
				anim.row = (dir.y > 0.f) ? 0 : 3; // down = 0, up = 3
			}
		}

		vel.value.x += (rand() % 3 - 1) * 0.1f;
		vel.value.y += (rand() % 3 - 1) * 0.1f;

		float len = std::sqrt(vel.value.x * vel.value.x + vel.value.y * vel.value.y);
		if (len > 0.f)
			vel.value /= len;
	}
}

entt::entity createNPC(entt::registry &registry, const sf::Vector2f &pos,
					   const sf::Vector2f &targetSize,
					   const std::unordered_map<int, AnimationClip> &clips,
					   float speed) {
	assert(!clips.empty() && "NPC must have at least one animation clip!");

	auto e = registry.create();
	registry.emplace<Position>(e, pos);
	registry.emplace<Speed>(e, speed);
	registry.emplace<Velocity>(e);

	Renderable render;
	render.textureName = clips.begin()->second.texture;
	render.textureRect = clips.begin()->second.frameRect;
	render.targetSize = targetSize;
	registry.emplace<Renderable>(e, std::move(render));

	Animation anim;
	anim.clips = clips;
	anim.state = clips.begin()->first;
	registry.emplace<Animation>(e, std::move(anim));

	return e;
}

entt::entity createStaticObject(entt::registry &registry, const sf::Vector2f &pos,
								const sf::Vector2f &targetSize,
								const std::string &textureName,
								const sf::IntRect &textureRect) {
	auto e = registry.create();
	registry.emplace<Position>(e, pos);
	registry.emplace<Velocity>(e, sf::Vector2f{0.f, 0.f});
	registry.emplace<Speed>(e, 0.f);

	Renderable render;
	render.textureName = textureName;
	render.textureRect = textureRect;
	render.targetSize = targetSize;
	registry.emplace<Renderable>(e, std::move(render));

	return e;
}

void weaponSystem(entt::registry &registry, const Input &input, float dt) {
	auto view = registry.view<Position, Weapon, Animation, PlayerControlled>();

	for (auto entity : view) {
		auto &pos = view.get<Position>(entity);
		auto &weapon = view.get<Weapon>(entity);
		auto &anim = view.get<Animation>(entity);

		weapon.timeSinceLastShot += dt;

		// Check for shooting input (Space key)
		bool wantsToShoot = input.isKeyDown(sf::Keyboard::Key::Space);

		if (wantsToShoot && weapon.timeSinceLastShot >= weapon.fireRate) {
			weapon.timeSinceLastShot = 0.f;

			// Get shooting direction based on animation direction
			sf::Vector2f shootDir = {1.f, 0.f}; // Default right
			switch (anim.row) {
			case 0: // Down
				shootDir = {0.f, 1.f};
				break;
			case 1: // Right
				shootDir = {1.f, 0.f};
				break;
			case 2: // Left
				shootDir = {-1.f, 0.f};
				break;
			case 3: // Up
				shootDir = {0.f, -1.f};
				break;
			}

			// Create bullet
			auto bullet = registry.create();
			sf::Vector2f bulletPos = pos.value + shootDir * 0.5f;
			registry.emplace<Position>(bullet, bulletPos);
			registry.emplace<Velocity>(bullet, shootDir);
			registry.emplace<Speed>(bullet, weapon.bulletSpeed);

			Renderable bulletRender;
			bulletRender.textureName = "game/assets/weapons/bullet.png";
			bulletRender.textureRect = sf::IntRect({0, 0}, {16, 16});
			bulletRender.targetSize = {16.f, 16.f};
			bulletRender.color = sf::Color(255, 220, 0, 255);
			registry.emplace<Renderable>(bullet, std::move(bulletRender));

			Projectile proj;
			proj.direction = shootDir;
			registry.emplace<Projectile>(bullet, std::move(proj));
			
			// Add damage component (use damage from weapon)
			Damage dmg;
			dmg.amount = weapon.damage;
			dmg.owner = entity;
			dmg.hasOwner = true;
			registry.emplace<Damage>(bullet, std::move(dmg));

			// Add shooting animation state
			registry.emplace_or_replace<IsShooting>(entity);
			
			// Add slight recoil effect
			if (auto vel = registry.try_get<Velocity>(entity)) {
				vel->value -= shootDir * 0.1f;
			}
		}

		// Handle shooting animation (visual effect only, no character animation change)
		if (registry.all_of<IsShooting>(entity)) {
			auto &shooting = registry.get<IsShooting>(entity);
			shooting.animationTime += dt;

			// Remove shooting state after animation completes (just for weapon visual effect)
			if (shooting.animationTime >= 0.3f) {
				registry.remove<IsShooting>(entity);
			}
		}
	}
}

void projectileSystem(entt::registry &registry, std::vector<engine::Tile> &tiles,
					  int worldWidth, int worldHeight, float dt) {
	auto view = registry.view<Position, Velocity, Speed, Projectile>();
	auto getIndex = [&](int x, int y) { return y * worldWidth + x; };

	std::vector<entt::entity> toDestroy;

	for (auto entity : view) {
		auto &pos = view.get<Position>(entity);
		const auto &vel = view.get<Velocity>(entity);
		const auto &speed = view.get<Speed>(entity);
		auto &proj = view.get<Projectile>(entity);

		// Update lifetime
		proj.timeAlive += dt;
		if (proj.timeAlive >= proj.lifetime) {
			toDestroy.push_back(entity);
			continue;
		}

		// Calculate new position
		sf::Vector2f newPos = pos.value + vel.value * speed.value * dt;

		// Check collision with solid tiles
		int tileX = static_cast<int>(newPos.x) - 1;
		int tileY = static_cast<int>(newPos.y);

		bool hitWall = false;
		if (tileX < 0 || tileX >= worldWidth || tileY < 0 || tileY >= worldHeight) {
			hitWall = true;
		} else if (tiles[getIndex(tileX, tileY)].solid) {
			hitWall = true;
		}

		if (hitWall) {
			toDestroy.push_back(entity);
			continue;
		}

		// Move bullet
		pos.value = newPos;
	}

	// Destroy expired projectiles
	for (auto entity : toDestroy) {
		registry.destroy(entity);
	}
}

void weaponDisplaySystem(entt::registry &registry, RenderFrame &frame,
						 const Camera &camera, ImageManager &imageManager) {
	auto view = registry.view<const Position, const WeaponDisplay, const Animation>();

	for (auto entity : view) {
		const auto &pos = view.get<const Position>(entity);
		const auto &weaponDisp = view.get<const WeaponDisplay>(entity);
		const auto &anim = view.get<const Animation>(entity);

		// Calculate weapon position based on entity position and direction
		sf::Vector2f weaponOffset = weaponDisp.offset;

		// Adjust offset based on direction
		switch (anim.row) {
		case 0: // Down
			weaponOffset = {0.2f, 0.3f};
			break;
		case 1: // Right
			weaponOffset = {0.4f, 0.f};
			break;
		case 2: // Left
			weaponOffset = {-0.4f, 0.f};
			break;
		case 3: // Up
			weaponOffset = {0.2f, -0.3f};
			break;
		}

		sf::Vector2f weaponWorldPos = pos.value + weaponOffset;

		// Determine weapon texture based on shooting state
		std::string weaponTexture = weaponDisp.textureName;
		if (registry.all_of<IsShooting>(entity)) {
			weaponTexture = weaponDisp.shootTextureName;
		}

		const sf::Image *weaponImage = &imageManager.getImage(weaponTexture);
		sf::IntRect weaponRect({0, 0}, {32, 32});

		// Get frame from shooting animation
		if (registry.all_of<IsShooting>(entity)) {
			const auto &weapon = registry.get<const Weapon>(entity);
			// Use frame based on animation
			int frameIdx = 0;
			if (auto shootingComp = registry.try_get<IsShooting>(entity)) {
				frameIdx = std::min(
					3, static_cast<int>(shootingComp->animationTime / 0.075f));
			}
			weaponRect.position.x = frameIdx * 32;
		}

		sf::Vector2f anchor = camera.worldToScreen(weaponWorldPos);
		float uniformScale = camera.zoom * 0.75f; // Scale weapon smaller

		sf::Vector2f weaponDrawPos =
			anchor - sf::Vector2f(weaponDisp.size.x * uniformScale * 0.5f,
								  weaponDisp.size.y * uniformScale);

		RenderFrame::SpriteData spriteData;
		spriteData.image = weaponImage;
		spriteData.textureRect = weaponRect;
		spriteData.scale = {uniformScale, uniformScale};
		spriteData.position = weaponDrawPos;
		spriteData.rotation = sf::degrees(0);
		spriteData.color = sf::Color::White;
		frame.sprites.push_back(spriteData);
	}
}

void damageSystem(entt::registry &registry) {
	auto projectiles = registry.view<const Position, const Projectile, const Damage>();
	auto damageable = registry.view<Position, Health>(entt::exclude<Dead>);
	
	std::vector<entt::entity> bulletsToDestroy;
	
	for (auto bullet : projectiles) {
		const auto &bulletPos = projectiles.get<Position>(bullet);
		const auto &damage = projectiles.get<Damage>(bullet);
		
		bool hitSomething = false;
		
		for (auto entity : damageable) {
			// Don't hit the shooter
			if (damage.hasOwner && damage.owner == entity) {
				continue;
			}
			
			const auto &entityPos = damageable.get<Position>(entity);
			auto &health = damageable.get<Health>(entity);
			
			// Check distance for hit detection
			sf::Vector2f diff = entityPos.value - bulletPos.value;
			float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y);
			
			if (distance < 0.6f) { // Hit radius
				float damageDealt = damage.amount;
				health.current -= damageDealt;
				
				if (health.current <= 0.f) {
					health.current = 0.f;
					health.isDead = true;
					registry.emplace<Dead>(entity);
				}
				
				// Create damage number if bullet was fired by player
				if (damage.hasOwner && registry.all_of<PlayerControlled>(damage.owner)) {
					auto damageNumber = registry.create();
					DamageNumber dmgNum;
					dmgNum.amount = damageDealt;
					dmgNum.position = entityPos.value;
					registry.emplace<DamageNumber>(damageNumber, dmgNum);
				}
				
				hitSomething = true;
				bulletsToDestroy.push_back(bullet);
				break;
			}
		}
	}
	
	// Destroy bullets that hit
	for (auto bullet : bulletsToDestroy) {
		registry.destroy(bullet);
	}
}

void healthBarSystem(entt::registry &registry, RenderFrame &frame,
					 const Camera &camera) {
	auto view = registry.view<const Position, const Health>(entt::exclude<Dead>);
	
	for (auto entity : view) {
		const auto &pos = view.get<Position>(entity);
		const auto &health = view.get<Health>(entity);
		
		// Health bar dimensions
		const float barWidth = 50.f;
		const float barHeight = 6.f;
		const float yOffset = -40.f; // Above entity
		
		sf::Vector2f screenPos = camera.worldToScreen(pos.value);
		sf::Vector2f barPos = screenPos + sf::Vector2f(-barWidth * 0.5f, yOffset);
		
		// Calculate health percentage
		float healthPercent = health.current / health.maximum;
		
		// Background (red)
		for (int y = 0; y < barHeight; ++y) {
			for (int x = 0; x < barWidth; ++x) {
				sf::Vertex v;
				v.position = {barPos.x + x, barPos.y + y};
				v.color = sf::Color(100, 0, 0, 200); // Dark red background
				frame.healthBarVertices.append(v);
			}
		}
		
		// Health fill (green to red gradient)
		float fillWidth = barWidth * healthPercent;
		sf::Color fillColor;
		
		if (healthPercent > 0.6f) {
			fillColor = sf::Color(0, 200, 0, 255); // Green
		} else if (healthPercent > 0.3f) {
			fillColor = sf::Color(200, 200, 0, 255); // Yellow
		} else {
			fillColor = sf::Color(200, 0, 0, 255); // Red
		}
		
		for (int y = 1; y < barHeight - 1; ++y) {
			for (int x = 1; x < fillWidth - 1; ++x) {
				sf::Vertex v;
				v.position = {barPos.x + x, barPos.y + y};
				v.color = fillColor;
				frame.healthBarVertices.append(v);
			}
		}
		
		// Border (black)
		for (int x = 0; x < barWidth; ++x) {
			// Top border
			sf::Vertex vTop;
			vTop.position = {barPos.x + x, barPos.y};
			vTop.color = sf::Color::Black;
			frame.healthBarVertices.append(vTop);
			
			// Bottom border
			sf::Vertex vBottom;
			vBottom.position = {barPos.x + x, barPos.y + barHeight - 1};
			vBottom.color = sf::Color::Black;
			frame.healthBarVertices.append(vBottom);
		}
		for (int y = 0; y < barHeight; ++y) {
			// Left border
			sf::Vertex vLeft;
			vLeft.position = {barPos.x, barPos.y + y};
			vLeft.color = sf::Color::Black;
			frame.healthBarVertices.append(vLeft);
			
			// Right border
			sf::Vertex vRight;
			vRight.position = {barPos.x + barWidth - 1, barPos.y + y};
			vRight.color = sf::Color::Black;
			frame.healthBarVertices.append(vRight);
		}
	}
}

void aiCombatSystem(entt::registry &registry, const Input &input, float dt) {
	auto aiEntities = registry.view<Position, Velocity, Animation, Weapon, AICombat, Health>(
		entt::exclude<PlayerControlled, Dead>);
	auto potentialTargets = registry.view<const Position, const Health>(entt::exclude<Dead>);
	
	for (auto aiEntity : aiEntities) {
		auto &pos = aiEntities.get<Position>(aiEntity);
		auto &vel = aiEntities.get<Velocity>(aiEntity);
		auto &anim = aiEntities.get<Animation>(aiEntity);
		auto &weapon = aiEntities.get<Weapon>(aiEntity);
		auto &combat = aiEntities.get<AICombat>(aiEntity);
		
		combat.shootCooldown += dt;
		
		// Find nearest enemy
		entt::entity nearestEnemy = entt::null;
		float nearestDistance = combat.detectionRange;
		
		for (auto target : potentialTargets) {
			if (target == aiEntity) continue; // Don't target self
			
			const auto &targetPos = potentialTargets.get<Position>(target);
			sf::Vector2f diff = targetPos.value - pos.value;
			float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y);
			
			if (distance < nearestDistance) {
				nearestDistance = distance;
				nearestEnemy = target;
			}
		}
		
		if (nearestEnemy != entt::null) {
			combat.target = nearestEnemy;
			combat.hasTarget = true;
			
			const auto &targetPos = registry.get<const Position>(nearestEnemy);
			sf::Vector2f diff = targetPos.value - pos.value;
			float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y);
			
			// Aim towards target
			if (distance > 0.01f) {
				sf::Vector2f direction = diff / distance;
				
				// Update animation direction
				if (std::abs(direction.x) > std::abs(direction.y)) {
					anim.row = (direction.x > 0.f) ? 1 : 2; // right=1, left=2
				} else {
					anim.row = (direction.y > 0.f) ? 0 : 3; // down=0, up=3
				}
				
				// Shoot if in range and cooldown is ready
				if (distance < combat.shootingRange && combat.shootCooldown >= combat.shootInterval) {
					combat.shootCooldown = 0.f;
					weapon.timeSinceLastShot = weapon.fireRate; // Force ready to shoot
					
					// Get shooting direction based on animation direction
					sf::Vector2f shootDir = direction;
					
					// Create bullet
					auto bullet = registry.create();
					sf::Vector2f bulletPos = pos.value + shootDir * 0.5f;
					registry.emplace<Position>(bullet, bulletPos);
					registry.emplace<Velocity>(bullet, shootDir);
					registry.emplace<Speed>(bullet, weapon.bulletSpeed);
					
					Renderable bulletRender;
					bulletRender.textureName = "game/assets/weapons/bullet.png";
					bulletRender.textureRect = sf::IntRect({0, 0}, {16, 16});
					bulletRender.targetSize = {16.f, 16.f};
					bulletRender.color = sf::Color(255, 100, 0, 255); // Orange bullets for AI
					registry.emplace<Renderable>(bullet, std::move(bulletRender));
					
					Projectile proj;
					proj.direction = shootDir;
					registry.emplace<Projectile>(bullet, std::move(proj));
					
					Damage dmg;
					dmg.amount = 10.f;
					dmg.owner = aiEntity;
					dmg.hasOwner = true;
					registry.emplace<Damage>(bullet, std::move(dmg));
					
					// Add shooting visual effect
					registry.emplace_or_replace<IsShooting>(aiEntity);
				}
			}
		} else {
			combat.hasTarget = false;
		}
	}
}

void deathSystem(entt::registry &registry) {
	auto deadEntities = registry.view<Dead, Renderable>();
	
	for (auto entity : deadEntities) {
		auto &render = deadEntities.get<Renderable>(entity);
		// Fade out dead entities
		if (render.color.a > 10) {
			render.color.a -= 5;
		} else {
			// Remove after fully faded
			registry.destroy(entity);
		}
	}
}

void damageNumberSystem(entt::registry &registry, float dt) {
	auto view = registry.view<DamageNumber>();
	std::vector<entt::entity> toDestroy;
	
	for (auto entity : view) {
		auto &dmgNum = view.get<DamageNumber>(entity);
		dmgNum.timeAlive += dt;
		
		// Remove after lifetime expires
		if (dmgNum.timeAlive >= dmgNum.lifetime) {
			toDestroy.push_back(entity);
		}
	}
	
	for (auto entity : toDestroy) {
		registry.destroy(entity);
	}
}

void damageNumberRenderSystem(entt::registry &registry, RenderFrame &frame,
							 const Camera &camera) {
	// Initialize vertex array for damage numbers
	frame.uiTextVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
	
	auto view = registry.view<const DamageNumber>();
	
	// Helper function to render a single digit
	auto renderDigit = [&](int digit, float x, float y, float pixelSize, sf::Color color) {
		// Simple 5x7 bitmap font for digits 0-9
		int digitData[10][7] = {
			{0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}, // 0
			{0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}, // 1
			{0b01110, 0b10001, 0b00001, 0b01110, 0b10000, 0b10000, 0b11111}, // 2
			{0b01110, 0b10001, 0b00001, 0b01110, 0b00001, 0b10001, 0b01110}, // 3
			{0b10001, 0b10001, 0b10001, 0b11111, 0b00001, 0b00001, 0b00001}, // 4
			{0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110}, // 5
			{0b01110, 0b10001, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}, // 6
			{0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}, // 7
			{0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}, // 8
			{0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b10001, 0b01110}  // 9
		};
		
		if (digit < 0 || digit > 9) return;
		
		const int *charData = digitData[digit];
		for (int row = 0; row < 7; ++row) {
			for (int col = 0; col < 5; ++col) {
				if (charData[row] & (1 << (4 - col))) {
					float px = x + col * pixelSize;
					float py = y + row * pixelSize;
					
					// Create two triangles for each pixel
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
	
	for (auto entity : view) {
		const auto &dmgNum = view.get<const DamageNumber>(entity);
		
		// Calculate screen position (float upward over time)
		float floatOffset = dmgNum.timeAlive * 2.0f; // Float upward
		sf::Vector2f worldPos = {
			dmgNum.position.x,
			dmgNum.position.y - floatOffset - 0.5f // Above entity
		};
		sf::Vector2f screenPos = camera.worldToScreen(worldPos);
		
		// Calculate alpha (fade out over time)
		float alpha = 1.0f - (dmgNum.timeAlive / dmgNum.lifetime);
		alpha = std::max(0.0f, std::min(1.0f, alpha));
		sf::Color textColor(255, 100, 100, static_cast<unsigned char>(255 * alpha)); // Red color
		
		// Convert damage amount to string and render
		int damageInt = static_cast<int>(dmgNum.amount);
		std::string damageStr = std::to_string(damageInt);
		
		float pixelSize = 2.5f;
		float charSpacing = 5.0f * pixelSize;
		float totalWidth = damageStr.length() * charSpacing;
		float startX = screenPos.x - totalWidth / 2.0f;
		float startY = screenPos.y;
		
		// Render each digit
		for (size_t i = 0; i < damageStr.length(); ++i) {
			int digit = damageStr[i] - '0';
			renderDigit(digit, startX + i * charSpacing, startY, pixelSize, textColor);
		}
	}
}

} // namespace systems
