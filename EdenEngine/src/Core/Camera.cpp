#include "Camera.h"

#include "Core/Input.h"

namespace Eden
{

	Camera::Camera(uint32_t screenWidth, uint32_t screenHeight)
	{
		position = { 0, 5, -10 };
		front = { 0, 0, 1 };
		target = { 0, 0, 0 };
		up = { 0, 1, 0 };
		m_LastX = (float)screenWidth / 2;
		m_LastY = (float)screenHeight / 2;
		m_Yaw = 0.0f;
		m_Pitch = 0.0f;
		m_Sensitivity = 0.2f;
		m_X = 0;
		m_Y = 0;
		m_FirstTimeMouse = true;
	}

	void Camera::Update(float deltaTime)
	{
		if (Input::GetMouseButton(MouseButton::RightButton))
		{
			Input::SetCursorMode(CursorMode::Hidden);

			m_Locked = true;

			float cameraSpeed = 10.0f * deltaTime;

			if (Input::GetKey(KeyCode::W))
				position -= cameraSpeed * front;
			if (Input::GetKey(KeyCode::S))
				position += cameraSpeed * front;
			if (Input::GetKey(KeyCode::D))
				position -= glm::normalize(glm::cross(front, up)) * cameraSpeed;
			if (Input::GetKey(KeyCode::A))
				position += glm::normalize(glm::cross(front, up)) * cameraSpeed;
			if (Input::GetKey(KeyCode::Space))
				position.y += cameraSpeed;
			if (Input::GetKey(KeyCode::LeftShift))
				position.y -= cameraSpeed;
		}
		else if (Input::GetMouseButtonUp(MouseButton::RightButton))
		{
			Input::SetCursorMode(CursorMode::Visible);

			m_Locked = false;
			m_FirstTimeMouse = true;
		}
	}

	void Camera::UpdateLookAt(float xPos, float yPos)
	{
		if (m_Locked)
		{
			if (m_FirstTimeMouse)
			{
				m_LastX = xPos;
				m_LastY = yPos;
				m_FirstTimeMouse = false;
			}

			float xoffset = xPos - m_LastX;
			float yoffset = m_LastY - yPos;
			m_LastX = xPos;
			m_LastY = yPos;
			xoffset *= m_Sensitivity;
			yoffset *= m_Sensitivity;

			m_Yaw += xoffset;
			m_Pitch += yoffset;

			if (m_Pitch > 89.f)
				m_Pitch = 89.f;
			if (m_Pitch < -89.f)
				m_Pitch = -89.f;

			front.x = -(sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch)));
			front.y = -(sin(glm::radians(m_Pitch)));
			front.z = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
			front = glm::normalize(front);
		}
	}
}