#pragma once
#include <string>
#include <ShlObj.h>
#include "ToolBoxPluginData.hpp"

class SmallToolBoxAction final : public candy::ActionBase
{
public:
	std::wstring id;
};
