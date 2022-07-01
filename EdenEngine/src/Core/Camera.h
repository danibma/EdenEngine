#pragma once

#include <glm/glm.hpp>

#include "Window.h"

namespace Eden
{
	class Camera
	{
	public:
		Camera() = default;
		Camera(uint32_t viewport_width, uint32_t viewport_height);

		void Update(float delta_time);
		void SetViewportSize(glm::vec2 size);
		void SetViewportPosition(glm::vec2 pos);

	private:
		void UpdateLookAt();

	public:
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
		glm::vec2 m_ViewportSize;
		glm::vec2 m_ViewportPosition = {0, 0};
	};

}
