#include "menu_loop.h"

#include "core/camera.h"
#include "core/engine.h"
#include "core/render.h"
#include "core/render_frame.h"
#include "loops/game_loop.h"
#include <SFML/Window/Event.hpp>
#include <memory>

MenuLoop::MenuLoop() {
	m_buttonSize = {200.f, 60.f};
	m_mouseWasPressed = false;
}

void MenuLoop::init() {
	m_engine = engine::Engine::get();
	// Set camera position to center of view for menu
	m_engine->camera.position = {m_engine->camera.size.x / 2.f, m_engine->camera.size.y / 2.f};
	// Center button on screen - use camera position as center
	m_buttonPos = m_engine->camera.position;
}

void MenuLoop::update(engine::Input &input, float dt) {
	// Get mouse position from window
	sf::Vector2i mousePixelPos = sf::Mouse::getPosition(m_engine->render.getWindow());
	
	// Get window size
	sf::Vector2u windowSize = m_engine->render.getWindow().getSize();
	
	// Convert screen pixel coordinates to view coordinates
	// The view is centered at camera.position with size camera.size
	// Map pixel coordinates (0,0 at top-left) to view coordinates
	sf::Vector2f viewTopLeft = {
		m_engine->camera.position.x - m_engine->camera.size.x / 2.f,
		m_engine->camera.position.y - m_engine->camera.size.y / 2.f
	};
	
	// Convert pixel coordinates to view coordinates
	sf::Vector2f mouseViewPos = {
		viewTopLeft.x + (static_cast<float>(mousePixelPos.x) / static_cast<float>(windowSize.x)) * m_engine->camera.size.x,
		viewTopLeft.y + (static_cast<float>(mousePixelPos.y) / static_cast<float>(windowSize.y)) * m_engine->camera.size.y
	};

	// Button position in view coordinates
	sf::Vector2f buttonTopLeft = {
		m_buttonPos.x - m_buttonSize.x / 2.f,
		m_buttonPos.y - m_buttonSize.y / 2.f
	};

	// Check if mouse is hovering over button (using view coordinates)
	m_buttonHovered = isPointInRect(mouseViewPos, buttonTopLeft, m_buttonSize);
	m_buttonClicked = false;

	// Check for mouse button press
	// Check current mouse button state and detect press (not hold)
	bool mouseIsPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
	
	if (mouseIsPressed && !m_mouseWasPressed && m_buttonHovered) {
		m_buttonClicked = true;
		// Switch to game loop
		// Don't set m_finished = true here, as the engine will handle the loop switch
		// The new loop will be set and this loop will be replaced
		auto gameLoop = std::make_unique<GameLoop>();
		m_engine->setLoop(std::move(gameLoop));
		// Note: We don't set m_finished here because the engine needs to continue running
		// The old loop (this one) will be destroyed when setLoop is called
		return;
	}
	
	m_mouseWasPressed = mouseIsPressed;
}

void MenuLoop::collectRenderData(engine::RenderFrame &frame,
								 engine::Camera &camera) {
	// Initialize vertex arrays
	frame.uiOverlayVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
	frame.uiOverlayVertices.clear();
	frame.uiTextVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
	frame.uiTextVertices.clear();

	// Render button
	renderButton(frame, camera, m_buttonPos, m_buttonSize, "Start game",
				 m_buttonHovered);
}

bool MenuLoop::isFinished() const { return m_finished; }

bool MenuLoop::isPointInRect(const sf::Vector2f &point, const sf::Vector2f &rectPos,
							 const sf::Vector2f &rectSize) const {
	return point.x >= rectPos.x && point.x <= rectPos.x + rectSize.x &&
		   point.y >= rectPos.y && point.y <= rectPos.y + rectSize.y;
}

void MenuLoop::renderButton(engine::RenderFrame &frame, engine::Camera &camera,
							const sf::Vector2f &buttonPos, const sf::Vector2f &buttonSize,
							const std::string &text, bool isHovered) {
	sf::Vector2f topLeft = {buttonPos.x - buttonSize.x / 2.f,
							buttonPos.y - buttonSize.y / 2.f};
	sf::Vector2f bottomRight = {buttonPos.x + buttonSize.x / 2.f,
								buttonPos.y + buttonSize.y / 2.f};

	// Button background color (darker when hovered)
	sf::Color buttonColor = isHovered ? sf::Color(100, 150, 200, 255)
									  : sf::Color(80, 120, 160, 255);
	sf::Color borderColor = sf::Color(200, 200, 200, 255);

	// Draw button background (two triangles)
	frame.uiOverlayVertices.append({topLeft, buttonColor});
	frame.uiOverlayVertices.append({{bottomRight.x, topLeft.y}, buttonColor});
	frame.uiOverlayVertices.append({bottomRight, buttonColor});

	frame.uiOverlayVertices.append({topLeft, buttonColor});
	frame.uiOverlayVertices.append({bottomRight, buttonColor});
	frame.uiOverlayVertices.append({{topLeft.x, bottomRight.y}, buttonColor});

	// Draw button border (simple outline)
	float borderWidth = 2.f;
	sf::Color border = borderColor;

	// Top border
	frame.uiOverlayVertices.append({topLeft, border});
	frame.uiOverlayVertices.append({{bottomRight.x, topLeft.y}, border});
	frame.uiOverlayVertices.append({{bottomRight.x, topLeft.y + borderWidth}, border});
	frame.uiOverlayVertices.append({topLeft, border});
	frame.uiOverlayVertices.append({{bottomRight.x, topLeft.y + borderWidth}, border});
	frame.uiOverlayVertices.append({{topLeft.x, topLeft.y + borderWidth}, border});

	// Bottom border
	frame.uiOverlayVertices.append({{topLeft.x, bottomRight.y - borderWidth}, border});
	frame.uiOverlayVertices.append({bottomRight, border});
	frame.uiOverlayVertices.append({{bottomRight.x, bottomRight.y - borderWidth}, border});
	frame.uiOverlayVertices.append({{topLeft.x, bottomRight.y - borderWidth}, border});
	frame.uiOverlayVertices.append({{bottomRight.x, bottomRight.y - borderWidth}, border});
	frame.uiOverlayVertices.append({{topLeft.x, bottomRight.y}, border});

	// Left border
	frame.uiOverlayVertices.append({topLeft, border});
	frame.uiOverlayVertices.append({{topLeft.x + borderWidth, topLeft.y}, border});
	frame.uiOverlayVertices.append({{topLeft.x + borderWidth, bottomRight.y}, border});
	frame.uiOverlayVertices.append({topLeft, border});
	frame.uiOverlayVertices.append({{topLeft.x + borderWidth, bottomRight.y}, border});
	frame.uiOverlayVertices.append({{topLeft.x, bottomRight.y}, border});

	// Right border
	frame.uiOverlayVertices.append({{bottomRight.x - borderWidth, topLeft.y}, border});
	frame.uiOverlayVertices.append({bottomRight, border});
	frame.uiOverlayVertices.append({{bottomRight.x, bottomRight.y}, border});
	frame.uiOverlayVertices.append({{bottomRight.x - borderWidth, topLeft.y}, border});
	frame.uiOverlayVertices.append({{bottomRight.x, bottomRight.y}, border});
	frame.uiOverlayVertices.append({{bottomRight.x - borderWidth, bottomRight.y}, border});

	// Render text on button
	float textPixelSize = 3.0f;
	// Calculate text width properly (accounting for UTF-8 and character spacing)
	float textWidth = calculateTextWidth(text, textPixelSize);
	sf::Vector2f textPos = {
		buttonPos.x - textWidth / 2.f,
		buttonPos.y - 3.5f * textPixelSize
	};
	renderText(frame, text, textPos, textPixelSize, sf::Color::White);
}

void MenuLoop::renderText(engine::RenderFrame &frame, const std::string &text,
						  const sf::Vector2f &pos, float pixelSize, sf::Color color) {
	// Simple 5x7 bitmap font data
	struct CharData {
		int data[7];
	};

	// UTF-8 font map: map UTF-8 byte sequences to character data
	// Latin letters for "Start game"
	std::map<std::string, CharData> utf8Font = {
		// Latin uppercase
		{"S", {{0b01110, 0b10001, 0b10000, 0b01110, 0b00001, 0b10001, 0b01110}}},
		{"G", {{0b01110, 0b10001, 0b10000, 0b10011, 0b10001, 0b10001, 0b01110}}},
		{"A", {{0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}}},
		{"M", {{0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}}},
		{"E", {{0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}}},
		{"T", {{0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}}},
		// Latin lowercase
		{"s", {{0b01110, 0b10001, 0b01110, 0b00001, 0b10001, 0b10001, 0b01110}}},
		{"t", {{0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00100, 0b00011}}},
		{"a", {{0b00000, 0b00000, 0b01110, 0b00001, 0b01111, 0b10001, 0b01111}}},
		{"r", {{0b00000, 0b00000, 0b10110, 0b11001, 0b10000, 0b10000, 0b10000}}},
		{"g", {{0b00000, 0b00000, 0b01111, 0b10001, 0b10001, 0b01111, 0b01110}}},
		{"e", {{0b00000, 0b00000, 0b01110, 0b10001, 0b11111, 0b10000, 0b01110}}},
		{"m", {{0b00000, 0b00000, 0b10110, 0b11001, 0b10001, 0b10001, 0b10001}}},
		// Space
		{" ", {{0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000}}},
		// Cyrillic (keeping for compatibility)
		{"\xD0\x9D", {{0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001, 0b10001}}}, // Н
		{"\xD0\xB0", {{0b00000, 0b00000, 0b01110, 0b10001, 0b10001, 0b10001, 0b01111}}}, // а
		{"\xD1\x87", {{0b00000, 0b00000, 0b10001, 0b10001, 0b10001, 0b10011, 0b01101}}}, // ч
		{"\xD1\x82", {{0b00000, 0b00000, 0b11111, 0b00100, 0b00100, 0b00100, 0b00100}}}, // т
		{"\xD1\x8C", {{0b00000, 0b00000, 0b10000, 0b10000, 0b11110, 0b10001, 0b11110}}}, // ь
		{"\xD0\xB8", {{0b00000, 0b00000, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001}}}, // и
		{"\xD0\xB3", {{0b00000, 0b00000, 0b11111, 0b10000, 0b10000, 0b10000, 0b10000}}}, // г
		{"\xD1\x80", {{0b00000, 0b00000, 0b11110, 0b10001, 0b11110, 0b10000, 0b10000}}}, // р
		{"\xD1\x83", {{0b00000, 0b00000, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}}}, // у
	};

	float charSpacing = 6.0f * pixelSize;
	float x = pos.x;
	size_t i = 0;

	while (i < text.length()) {
		std::string charStr;
		
		// Extract UTF-8 character (1-4 bytes)
		unsigned char firstByte = static_cast<unsigned char>(text[i]);
		if (firstByte < 0x80) {
			// ASCII character
			charStr = text.substr(i, 1);
			i += 1;
		} else if ((firstByte & 0xE0) == 0xC0) {
			// 2-byte UTF-8 character
			charStr = text.substr(i, 2);
			i += 2;
		} else if ((firstByte & 0xF0) == 0xE0) {
			// 3-byte UTF-8 character
			charStr = text.substr(i, 3);
			i += 3;
		} else if ((firstByte & 0xF8) == 0xF0) {
			// 4-byte UTF-8 character
			charStr = text.substr(i, 4);
			i += 4;
		} else {
			// Invalid UTF-8, skip
			i += 1;
			continue;
		}

		if (utf8Font.find(charStr) == utf8Font.end()) {
			x += charSpacing;
			continue;
		}

		const CharData &charData = utf8Font[charStr];
		for (int row = 0; row < 7; ++row) {
			for (int col = 0; col < 5; ++col) {
				if (charData.data[row] & (1 << (4 - col))) {
					float px = x + col * pixelSize;
					float py = pos.y + row * pixelSize;

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
		x += charSpacing;
	}
}

float MenuLoop::calculateTextWidth(const std::string &text, float pixelSize) const {
	float charSpacing = 6.0f * pixelSize;
	float width = 0.0f;
	size_t i = 0;

	while (i < text.length()) {
		std::string charStr;
		
		// Extract UTF-8 character (1-4 bytes)
		unsigned char firstByte = static_cast<unsigned char>(text[i]);
		if (firstByte < 0x80) {
			// ASCII character
			charStr = text.substr(i, 1);
			i += 1;
		} else if ((firstByte & 0xE0) == 0xC0) {
			// 2-byte UTF-8 character
			charStr = text.substr(i, 2);
			i += 2;
		} else if ((firstByte & 0xF0) == 0xE0) {
			// 3-byte UTF-8 character
			charStr = text.substr(i, 3);
			i += 3;
		} else if ((firstByte & 0xF8) == 0xF0) {
			// 4-byte UTF-8 character
			charStr = text.substr(i, 4);
			i += 4;
		} else {
			// Invalid UTF-8, skip
			i += 1;
			continue;
		}

		// Each character takes charSpacing width
		width += charSpacing;
	}

	return width;
}

