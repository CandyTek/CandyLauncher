#pragma once
#include <string>
#include "plugins/sdk/CandyPluginSdk.hpp"

class BookmarkAction final : public candy::ActionBase {
public:
	std::wstring url;
};
