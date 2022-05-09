#pragma once

#include <glm/glm.hpp>

#include "Window.h"

namespace Eden
{
	class Camera
	{
	public:
		Camera() = default;
		Camera(uint32_t screenWidth, uint32_t screenHeight);

		void Update(Window* window, float deltaTime);
		void UpdateLookAt(Window* window);

		glm::vec3 position;
		glm::vec3 front;
		glm::vec3 up;

	private:
		float m_LastX, m_LastY;
		float m_Yaw, m_Pitch;
		float m_Sensitivity;
		float m_X, m_Y;
		bool m_FirstTimeMouse = true;
		bool m_Locked = false;
	};

}
