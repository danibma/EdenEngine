#pragma once

#include <glm/glm.hpp>

#include "Window.h"

namespace Eden
{
	class Camera
	{
	public:
		Camera() = default;
		Camera(uint32_t screen_width, uint32_t screen_height);

		void Update(Window* window, float delta_time);
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
