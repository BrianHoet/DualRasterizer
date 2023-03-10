#pragma once
#include <cassert>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>

#include "Math.h"
#include "Timer.h"

namespace dae
{
	struct Camera
	{
		Camera() = default;

		Camera(const Vector3& _origin, float _fovAngle, float ratio):
			origin{ _origin },
			fovAngle{ _fovAngle },
			aspectRatio{ ratio }
		{
		}


		Vector3 origin{};
		float fovAngle{ 45.f };
		float fov{ tanf((fovAngle * TO_RADIANS) / 2.f) };
		float aspectRatio{ 1.f };

		Vector3 forward{ Vector3::UnitZ };
		Vector3 up{ Vector3::UnitY };
		Vector3 right{ Vector3::UnitX };

		float totalPitch{};
		float totalYaw{};

		Matrix invViewMatrix{};
		Matrix viewMatrix{};
		Matrix projectionMatrix{};

		const float nearPlane{ 0.1f };
		const float farPlane{ 100.f };

		void Initialize(float _fovAngle, Vector3 _origin, float ratio)
		{
			fovAngle = _fovAngle;
			fov = tanf((fovAngle * TO_RADIANS) / 2.f);
			aspectRatio = ratio;

			origin = _origin;
		}

		void CalculateViewMatrix()
		{
			viewMatrix = Matrix::CreateLookAtLH(origin, forward, up);
		}

		void CalculateProjectionMatrix()
		{
			projectionMatrix = Matrix::CreatePerspectiveFovLH(fov, aspectRatio, nearPlane, farPlane);
		}

		const Matrix& GetViewMatrix() const
		{
			return viewMatrix;
		}

		const Matrix& GetInverseViewMatrix() const
		{
			return invViewMatrix;
		}

		const Matrix& GetProjectionMatrix() const
		{
			return projectionMatrix;
		}

		Matrix GetWorldViewProjection() const
		{
			return GetViewMatrix() * GetProjectionMatrix();
		}

		void Update(const Timer* pTimer)
		{
			//Camera Update Logic
			 float movementSpeed{ 50 * pTimer->GetElapsed() };

			const float speedMultiplier{ 3 };

			const Uint8* pStates{ SDL_GetKeyboardState(nullptr) };
			if (pStates[SDL_SCANCODE_LSHIFT] || pStates[SDL_SCANCODE_RSHIFT])
			{
				movementSpeed *= speedMultiplier;
			}
			if (pStates[SDL_SCANCODE_W] || pStates[SDL_SCANCODE_UP])
			{
				origin += forward * movementSpeed;
			}
			if (pStates[SDL_SCANCODE_S] || pStates[SDL_SCANCODE_DOWN])
			{
				origin -= forward * movementSpeed;
			}
			if (pStates[SDL_SCANCODE_D] || pStates[SDL_SCANCODE_RIGHT])
			{
				origin += right * movementSpeed;
			}
			if (pStates[SDL_SCANCODE_A] || pStates[SDL_SCANCODE_LEFT])
			{
				origin -= right * movementSpeed;
			}

			

			//Mouse Input
			int mouseX{}, mouseY{};
			const uint32_t mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);

			
			const float rotationSpeed{ 1.f * pTimer->GetElapsed() };

			if ((mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) && (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT)))
			{
				origin.y += -mouseY * movementSpeed;
			}
			else if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT))
			{
				totalPitch += -mouseY * rotationSpeed;
				totalYaw += mouseX * rotationSpeed;
			}
			else if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT))
			{
				totalYaw += mouseX * rotationSpeed;

				origin.z += -mouseY * movementSpeed;
			}

			const Matrix finalRotation{ Matrix::CreateRotationX(totalPitch) * Matrix::CreateRotationY(totalYaw) };
			forward = finalRotation.TransformVector(Vector3::UnitZ);
			forward.Normalize();

			//if rotate camera goes left and right in world
			right = Vector3::Cross(up, forward);

			//Update Matrices
			CalculateViewMatrix();
			CalculateProjectionMatrix();
		}
	};
}
