#pragma once
#include <string>

#include "PluginBrowserPluginData.hpp"
#include "../../util/BitmapUtil.hpp"

class PluginBrowserAction final : public candy::ActionBase {
public:
	std::wstring packageName;
	bool enabled = false;
	bool canToggle = true;
};
