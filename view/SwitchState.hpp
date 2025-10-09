#pragma once

#include <Shobjidl.h>


struct SwitchState {
	HWND hwnd;
	bool isOn;
	bool isHovered;
	bool isPressed;
	bool isAnimating;
	float animationProgress; // 0.0f to 1.0f
	DWORD animationStartTime;
};
