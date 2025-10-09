#pragma once
#include "FeatureLaunchPluginData.hpp"
#include "plugins/BaseLaunchAction.hpp"

class FeatureLaunchAction final : public BaseLaunchAction {
public:

	FeatureLaunchAction(const std::wstring& title2, const std::wstring& subTitle2, const std::wstring& icon,
		const CallbackFunction& callback) : BaseLaunchAction(title2, subTitle2, icon, callback) {
		matchText=m_host->GetTheProcessedMatchingText(title2);
		pluginId=m_pluginId;
	}

	FeatureLaunchAction(const std::wstring& title2, const std::wstring& subTitle2, int loadType, int index,
	const CallbackFunction& callback) : BaseLaunchAction(title2, subTitle2, L"", callback) {
		matchText=m_host->GetTheProcessedMatchingText(title2);
		if (loadType == LOADTYPE_APP_RES) {
			iconBitmap = m_host->LoadResIconAsBitmap(index, 48, 48);
		} else if (loadType == LOADTYPE_SHELL32) {
			iconBitmap = LoadShell32IconAsBitmap(index);
		}
		matchText=m_host->GetTheProcessedMatchingText(title2);
		pluginId=m_pluginId;
	}
};
