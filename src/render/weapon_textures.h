#pragma once

#include <string_view>

namespace render {

inline constexpr std::string_view MAGIC_BALL_TEXTURE = "assets/runtime/projectiles/magic_ball.png";
inline constexpr std::string_view SWORD_RING_TEXTURE = "assets/runtime/projectiles/sword_ring.png";

// Generate textures for MagicStick and Sword on disk.
// Always overwrites existing files with procedurally generated images.
// Default sizes: 32x32 for magic ball, 64x64 for sword ring.
void generateWeaponTextures(unsigned magicBallSize, unsigned swordRingSize);

inline void generateWeaponTextures() { generateWeaponTextures(32u, 64u); }

} // namespace render
