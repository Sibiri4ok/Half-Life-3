#include "render/weapon_textures.h"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace render {

namespace {

void makeMagicBallImage(const std::filesystem::path &path, unsigned size) {
  using namespace sf;

  if (size == 0u)
    size = 1u;

  Image img({size, size}, Color::Transparent);
  Vector2f center{size / 2.f, size / 2.f};
  float outerRadius = size * 0.45f;
  float innerRadius = outerRadius - 1.5f; // thin black border

  float outerSq = outerRadius * outerRadius;
  float innerSq = innerRadius * innerRadius;

  const Color baseColor(60, 150, 255);
  const Vector2f lightDir = {-0.7f, -0.7f}; // light from top-left
  for (unsigned y = 0; y < size; ++y) {
    for (unsigned x = 0; x < size; ++x) {
      Vector2f p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
      float dx = p.x - center.x;
      float dy = p.y - center.y;
      float d2 = dx * dx + dy * dy;

      if (d2 > outerSq)
        continue;

      if (d2 >= innerSq) {
        img.setPixel({x, y}, Color::Black);
        continue;
      }

      float r = std::sqrt(d2) / outerRadius;

      float len = std::sqrt(dx * dx + dy * dy);
      Vector2f dirNorm = len > 0.f ? Vector2f{dx / len, dy / len} : Vector2f{0.f, 0.f};
      float ndotl = -(dirNorm.x * lightDir.x + dirNorm.y * lightDir.y);
      ndotl = std::max(0.f, ndotl);

      float angle = std::atan2(dy, dx);
      float facet = std::cos(6.f * angle); // six facets

      float intensity = 0.65f + 0.25f * (1.f - r) + 0.45f * ndotl + 0.18f * facet;

      intensity = std::clamp(intensity, 0.25f, 1.25f);

      auto shade = [&](std::uint8_t c) {
        float v = static_cast<float>(c) * intensity;
        if (v > 255.f)
          v = 255.f;
        return static_cast<std::uint8_t>(v);
      };

      Color finalColor(shade(baseColor.r), shade(baseColor.g), shade(baseColor.b), 255);

      if (ndotl > 0.85f && r < 0.55f) {
        finalColor = Color(240, 250, 255, 255);
      }

      if (ndotl < 0.1f && r > 0.4f) {
        finalColor.r = static_cast<std::uint8_t>(finalColor.r * 0.6f);
        finalColor.g = static_cast<std::uint8_t>(finalColor.g * 0.6f);
        finalColor.b = static_cast<std::uint8_t>(finalColor.b * 0.75f);
      }

      float edge = std::abs(std::sin(3.f * angle));
      if (edge > 0.9f && r > 0.3f && r < 0.9f) {
        finalColor.r = static_cast<std::uint8_t>(finalColor.r * 0.9f);
        finalColor.g = static_cast<std::uint8_t>(finalColor.g * 0.9f);
        finalColor.b = static_cast<std::uint8_t>(finalColor.b * 0.95f);
      }

      img.setPixel({x, y}, finalColor);
    }
  }

  bool ok = img.saveToFile(path);
  (void)ok;
}

void makeSwordRingImage(const std::filesystem::path &path, unsigned size) {
  using namespace sf;

  if (size == 0u)
    size = 1u;

  Image img({size, size}, Color::Transparent);

  Vector2f center{size / 2.f, size / 2.f};
  float outerRadius = size / 2.f;
  float innerRadius = outerRadius * 0.97f;

  float outerSq = outerRadius * outerRadius;
  float innerSq = innerRadius * innerRadius;

  for (unsigned y = 0; y < size; ++y) {
    for (unsigned x = 0; x < size; ++x) {
      Vector2f p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
      float dx = p.x - center.x;
      float dy = p.y - center.y;
      float d2 = dx * dx + dy * dy;

      if (d2 > outerSq || d2 < innerSq)
        continue;

      float dist = std::sqrt(d2);
      float t = (dist - innerRadius) / (outerRadius - innerRadius);
      if (t < 0.f)
        t = 0.f;
      if (t > 1.f)
        t = 1.f;

      float alpha = 190.f + (1.f - t) * 65.f;
      img.setPixel({x, y}, Color(255, 255, 255, static_cast<std::uint8_t>(alpha)));
    }
  }

  bool ok = img.saveToFile(path);
  (void)ok;
}

} // namespace

void generateWeaponTextures(unsigned magicBallSize, unsigned swordRingSize) {
  namespace fs = std::filesystem;

  const fs::path magicPath{MAGIC_BALL_TEXTURE};
  const fs::path swordPath{SWORD_RING_TEXTURE};

  fs::create_directories(magicPath.parent_path());

  makeMagicBallImage(magicPath, magicBallSize);
  makeSwordRingImage(swordPath, swordRingSize);
}

} // namespace render
