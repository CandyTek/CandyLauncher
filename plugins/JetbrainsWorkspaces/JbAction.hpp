#pragma once
#include <string>
#include "JbPluginData.hpp"

class JbAction final : public candy::ActionBase {
public:
	// Additional data for JetBrains workspace
	std::wstring projectPath; // Full path to the project
	std::wstring ideName; // Name of the IDE
};
