#pragma once

#include "AirPodsPluginData.hpp"

class AirPodsAction final : public candy::ActionBase {
public:
	// Shown by OnActionExecute / copied by Ctrl+C: title + subtitle
	std::wstring details;
};
