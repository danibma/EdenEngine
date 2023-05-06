#include "Camera.h"

#include "Core/Input.h"
#include "Log.h"

namespace Eden
{

	Camera::Camera(const uint32_t viewportWidth, const uint32_t viewportHeight)
	{
		position = { 0, 8, -19 };
		front = { 0, 0, 1 };
		up = { 0, 1, 0 };
		m_LastX = (float)viewportWidth / 2;
		m_LastY = (float)viewportHeight / 2;
		m_Yaw = 0.0f;
		m_Pitch = 0.0f;
		m_Sensitivity = 0.2f;
		m_X = 0;
		m_Y = 0;
		m_bFirstTimeMouse = true;
		m_ViewportSize = { viewportWidth, viewportHeight };
	}

	void Camera::Update(float deltaTime)
	{
		if (Input::GetInputMode() == InputMode::UI)
			return;

		if (Input::GetMouseButton(MouseButton::RightButton))
		{
			Input::SetCursorMode(CursorMode::Hidden);
			int64_t viewportX = static_cast<int64_t>((m_ViewportSize.x / 2) + m_ViewportPosition.x);
			int64_t viewportY = static_cast<int64_t>((m_ViewportSize.y / 2) + m_ViewportPosition.y);
			Input::SetMousePos(viewportX, viewportY);

			m_bIsLocked = true;

			float cameraSpeed = 10.0f * deltaTime;

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

			m_bIsLocked = false;
			m_bFirstTimeMouse = true;
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
		auto[xPos, yPos] = Input::GetMousePos();

		if (m_bIsLocked)
		{
			if (m_bFirstTimeMouse)
			{
				m_LastX = (float)xPos;
				m_LastY = (float)yPos;
				m_bFirstTimeMouse = false;
			}

			float xoffset = xPos - m_LastX;
			float yoffset = m_LastY - yPos;
			m_LastX = (float)xPos;
			m_LastY = (float)yPos;
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