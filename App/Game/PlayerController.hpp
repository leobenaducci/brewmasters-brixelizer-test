#pragma once
#include <Windows.h>
#include <Camera.hpp>
#include <Input/UserInput.hpp>
#include <iostream>

class PlayerController {
public:
	PlayerController(Camera& camera, Input::UserInput& input)
		: m_Camera(camera), m_Input(input) {}

	void Update(float dt) noexcept {
		auto [mdx, mdy] = m_Input.GetMouseDelta();
		
		if (m_Input.IsMouseButtonHeld(VK_RBUTTON)) {
			m_Camera.Rotate(mdx, mdy);
		}

		float const fwd  = KeyAxis('W', 'S');
		float const side = KeyAxis('A', 'D');
		float const up   = KeyAxis('E', 'Q');

		m_Camera.Move(fwd, side, up, dt);
	}

private:
	float KeyAxis(int pos, int neg) const noexcept {
		float axis = 0.0f;

		if (m_Input.IsKeyHeld(pos)) axis += 1.0f;
		if (m_Input.IsKeyHeld(neg)) axis -= 1.0f;

		return axis;
	}

private:
	Camera& m_Camera;
	Input::UserInput& m_Input;
};