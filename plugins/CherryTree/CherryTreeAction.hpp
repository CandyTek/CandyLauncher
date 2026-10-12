#pragma once
#include <string>
#include "CherryTreePluginData.hpp"

class CherryTreeAction final : public candy::ActionBase {
public:
	int nodeId = 0;
	std::wstring url;
	std::wstring text;
};
