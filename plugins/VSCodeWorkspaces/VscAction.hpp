#pragma once
#include <string>
#include "VscPluginData.hpp"

class VscAction final : public candy::ActionBase {
public:
	// Additional data for workspace
	std::wstring projectPath; // Full path to the project
	std::wstring originalUri; // Original VSCode URI for launching
	bool isWorkspaceFile = false; // Whether it's a .code-workspace file
};
