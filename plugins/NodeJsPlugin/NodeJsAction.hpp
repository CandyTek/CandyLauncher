#pragma once
#include "NodeJsPluginData.hpp"
#include "../../util/json.hpp"

// JS 插件返回的一条结果。图标在发布前由 NodeJsPlugin 填好：
// 图片文件用 iconBitmap + iconKey（同一路径共享一份位图），其它文件交给主程序按 iconPath 延迟解析
class NodeJsAction final : public candy::ActionBase {
public:
    std::wstring jsPluginId;
    nlohmann::json actionData;
};
