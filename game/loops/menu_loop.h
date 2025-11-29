#pragma once

#include "core/loop.h"
#include <SFML/Graphics/VertexArray.hpp>

namespace engine {
struct Engine;
struct Camera;
struct RenderFrame;
} // namespace engine

/**
 * @brief Main menu loop implementation.
 *
 * Displays a start menu with a "Начать игру" (Start Game) button.
 * Handles mouse clicks to start the game.
 */
class MenuLoop : public engine::ILoop {
  public:
	/**
	 * @brief Constructs menu loop.
	 */
	MenuLoop();
	virtual ~MenuLoop() = default;

	/**
	 * @brief Initializes the menu state.
	 */
	void init() override;

	/**
	 * @brief Updates menu logic and handles input.
	 * @param input Reference to the input system for user interaction.
	 * @param dt Delta time in seconds since last update.
	 */
	void update(engine::Input &input, float dt) override;

	/**
	 * @brief Collects render data for the menu screen.
	 * @param frame Reference to the render frame to populate with draw commands.
	 * @param camera Reference to the camera for view-relative rendering.
	 */
	void collectRenderData(engine::RenderFrame &frame,
						   engine::Camera &camera) override;

	/**
	 * @brief Checks if the menu loop should terminate.
	 * @return True if the menu loop has finished execution, false otherwise.
	 */
	bool isFinished() const override;

  private:
	engine::Engine *m_engine = nullptr; ///< Pointer to the main engine instance
	
	/**
	 * @brief Checks if a point is inside a rectangle.
	 * @param point Point to check.
	 * @param rectPos Top-left corner of rectangle.
	 * @param rectSize Size of rectangle.
	 * @return True if point is inside rectangle.
	 */
	bool isPointInRect(const sf::Vector2f &point, const sf::Vector2f &rectPos,
					   const sf::Vector2f &rectSize) const;

	/**
	 * @brief Renders a button with text.
	 * @param frame Reference to the render frame.
	 * @param camera Reference to the camera.
	 * @param buttonPos Position of the button center.
	 * @param buttonSize Size of the button.
	 * @param text Text to display on button.
	 * @param isHovered Whether the button is being hovered.
	 */
	void renderButton(engine::RenderFrame &frame, engine::Camera &camera,
					  const sf::Vector2f &buttonPos, const sf::Vector2f &buttonSize,
					  const std::string &text, bool isHovered);

	/**
	 * @brief Renders text using pixel-based font.
	 * @param frame Reference to the render frame.
	 * @param text Text to render.
	 * @param pos Position to render text at (top-left).
	 * @param pixelSize Size of each pixel.
	 * @param color Color of the text.
	 */
	void renderText(engine::RenderFrame &frame, const std::string &text,
					const sf::Vector2f &pos, float pixelSize, sf::Color color);

	/**
	 * @brief Calculates the width of text in pixels.
	 * @param text Text to measure.
	 * @param pixelSize Size of each pixel.
	 * @return Width of the text in pixels.
	 */
	float calculateTextWidth(const std::string &text, float pixelSize) const;

	sf::Vector2f m_buttonPos;	 ///< Center position of the start button
	sf::Vector2f m_buttonSize;	 ///< Size of the start button
	bool m_buttonHovered = false; ///< Whether button is being hovered
	bool m_buttonClicked = false; ///< Whether button was clicked this frame
	bool m_mouseWasPressed = false; ///< Previous frame mouse button state
};

