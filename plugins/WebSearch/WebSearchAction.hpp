#pragma once
#include <string>
#include "plugins/sdk/CandyPluginSdk.hpp"

class WebSearchAction final : public candy::ActionBase {
public:
	std::wstring sourceUrl = L"";
	std::wstring searchUrl = L"";
	uint16_t custom_action_id = 0;
};
