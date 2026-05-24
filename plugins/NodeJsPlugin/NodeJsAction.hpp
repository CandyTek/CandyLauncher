#pragma once
#include "../BaseAction.hpp"
#include "../../util/json.hpp"
#include "util/BitmapUtil.hpp"

class NodeJsAction : public BaseAction {
public:
    std::wstring title;
    std::wstring subTitle;
    std::wstring iconFilePath;
    int iconFilePathIndex = -1;
    HBITMAP iconBitmap = nullptr;

    std::wstring jsPluginId;
    nlohmann::json actionData;

    NodeJsAction() = default;

    ~NodeJsAction() override {
        if (iconBitmap) {
            DeleteObject(iconBitmap);
        }
    }

    std::wstring& getTitle() override {
        return title;
    }

    std::wstring& getSubTitle() override {
        return subTitle;
    }

    std::wstring& getIconFilePath() override {
        return iconFilePath;
    }

    int getIconFilePathIndex() override {
        return iconFilePathIndex;
    }

    HBITMAP getIconBitmap() override {
        if (iconFilePathIndex != -1) {
            return nullptr;
        }
        if (iconBitmap == nullptr && !iconFilePath.empty()) {
            if (EndsWithAnyIgnoreCase(iconFilePath, {L".ico",L".png",L".jpeg",L".jpg",L".bmp",L".tiff",L".gif"})) {
                iconBitmap = LoadPngAsHBITMAP(iconFilePath.c_str(), 48, 48);
            } else if (EndsWithIgnoreCase(iconFilePath, L".svg")) {
                iconBitmap = LoadSvgAsHBITMAP(iconFilePath.c_str(), 48, 48);
            } else {
                iconFilePathIndex = GetSysImageIndex(iconFilePath);
                return nullptr;
            }
        }

        return iconBitmap;
    }
};
