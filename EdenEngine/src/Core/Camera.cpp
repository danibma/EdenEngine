#include "Camera.h"

#include "Core/Input.h"
#include "Log.h"

namespace Eden
{

	Camera::Camera(const uint32_t viewport_width, const uint32_t viewport_height)
	{
		position = { 0, 3, -10 };
		front = { 0, 0, 1 };
		up = { 0, 1, 0 };
		m_LastX = (float)viewport_width / 2;
		m_LastY = (float)viewport_height / 2;
		m_Yaw = 0.0f;
		m_Pitch = 0.0f;
		m_Sensitivity = 0.2f;
		m_X = 0;
		m_Y = 0;
		m_FirstTimeMouse = true;
		m_ViewportSize = { viewport_width, viewport_height };
	}

	void Camera::Update(float delta_time)
	{
		if (Input::GetInputMode() == InputMode::UI)
			return;

		if (Input::GetMouseButton(MouseButton::RightButton))
		{
			Input::SetCursorMode(CursorMode::Hidden);
			int64_t viewport_x = static_cast<int64_t>((m_ViewportSize.x / 2) + m_ViewportPosition.x);
			int64_t viewport_y = static_cast<int64_t>((m_ViewportSize.y / 2) + m_ViewportPosition.y);
			Input::SetMousePos(viewport_x, viewport_y);

			m_Locked = true;

			float cameraSpeed = 10.0f * delta_time;

			if (Input::GetKey(KeyCode::W))
				position += cameraSpeed * front;
			if (Input::GetKey(KeyCode::S))
				position -= cameraSpeed * front;
			if (Input::GetKey(KeyCode::D))
				position -= glm::normalize(glm::cross(front, up)) * cameraSpeed;
			if (Input::GetKey(KeyCode::A))
				position += glm::normalize(glm::cross(front, up)) * cameraSpeed;
			if (Input::GetKey(KeyCode::Space))
				position.y += cameraSpeed;
			if (Input::GetKey(KeyCode::Shift))
				position.y -= cameraSpeed;

			UpdateLookAt();
		}
		else if (Input::GetMouseButtonUp(MouseButton::RightButton))
		{
			Input::SetCursorMode(CursorMode::Visible);

			m_Locked = false;
			m_FirstTimeMouse = true;
		}
	}

	void Camera::SetViewportSize(glm::vec2 size)
	{
		m_ViewportSize = size;
	}

	void Camera::SetViewportPosition(glm::vec2 pos)
	{
		m_ViewportPosition = pos;
	}

	void Camera::UpdateLookAt()
{
		auto[x_pos, y_pos] = Input::GetMousePos();

		if (m_Locked)
		{
			if (m_FirstTimeMouse)
			{
				m_LastX = (float)x_pos;
				m_LastY = (float)y_pos;
				m_FirstTimeMouse = false;
			}

			float xoffset = x_pos - m_LastX;
			float yoffset = m_LastY - y_pos;
			m_LastX = (float)x_pos;
			m_LastY = (float)y_pos;
			xoffset *= m_Sensitivity;
			yoffset *= m_Sensitivity;

			m_Yaw += xoffset;
			m_Pitch += yoffset;

			if (m_Pitch > 89.f)
				m_Pitch = 89.f;
			if (m_Pitch < -89.f)
				m_Pitch = -89.f;

			front.x = (sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch)));
			front.y = (sin(glm::radians(m_Pitch)));
			front.z = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
			front = glm::normalize(front);
		}
	}
}