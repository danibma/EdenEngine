#pragma once

#include <glm/glm.hpp>

#include "Window.h"

namespace Eden
{
	class Camera
	{
	public:
		Camera() = default;
		Camera(uint32_t viewportWidth, uint32_t viewportHeight);

		void Update(float deltaTime);
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
		bool m_bFirstTimeMouse = true;
		bool m_bIsLocked = false;
		glm::vec2 m_ViewportSize;
		glm::vec2 m_ViewportPosition = {0, 0};
	};

}
