#pragma once
#include <string>
#include "plugins/sdk/CandyPluginSdk.hpp"

class ExampleAction final : public candy::ActionBase {
public:
	std::wstring id;
};
