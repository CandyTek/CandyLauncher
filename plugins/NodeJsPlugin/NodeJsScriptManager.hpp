#pragma once
#include <string>
#include <vector>
#include <filesystem>

#include "NodeJsPluginData.hpp"
#include "../../util/json.hpp"
#include "../../util/FileUtil.hpp"
#include "../../util/LogUtil.hpp"
#include "util/BitmapUtil.hpp"

class NodeJsScript {
private:
    std::wstring m_dir;
    std::wstring m_id;
    std::wstring m_name;
    std::wstring m_entry;
    std::vector<std::wstring> m_triggerKeywords;

public:

    NodeJsScript(const std::wstring& dir, const nlohmann::json& j) : m_dir(dir) {
        // Handle both standard and Flow Launcher formats
        if (j.contains("Id") && j["Id"].is_string()) m_id = utf8_to_wide(j["Id"]);
        else if (j.contains("ID") && j["ID"].is_string()) m_id = utf8_to_wide(j["ID"]);

        if (j.contains("Name") && j["Name"].is_string()) m_name = utf8_to_wide(j["Name"]);

        if (j.contains("Entry") && j["Entry"].is_string()) m_entry = utf8_to_wide(j["Entry"]);
        else if (j.contains("ExecuteFileName") && j["ExecuteFileName"].is_string()) m_entry = utf8_to_wide(j["ExecuteFileName"]);

        const auto normalize_keyword = [](std::wstring kw) {
            if (!kw.empty() && !iswspace(kw.back())) {
                kw += L' ';
            }
            return kw;
        };

        if (j.contains("TriggerKeywords") && j["TriggerKeywords"].is_array()) {
            for (const auto& k : j["TriggerKeywords"]) {
                if (k.is_string()) {
                    auto kw = utf8_to_wide(k);
                    m_triggerKeywords.push_back(normalize_keyword(kw));
                }
            }
        } else if (j.contains("ActionKeyword") && j["ActionKeyword"].is_string()) {
            auto kw = utf8_to_wide(j["ActionKeyword"]);
            m_triggerKeywords.push_back(normalize_keyword(kw));
        }
        if (j.contains("IcoPath") && j["IcoPath"].is_string()) {
            std::wstring icoPath = utf8_to_wide(j["IcoPath"]);
            std::wstring iconFilePath;

            if (icoPath.find(L":\\") == std::wstring::npos &&
                icoPath.find(L'/') != 0) {
                iconFilePath = m_dir + L"\\" + icoPath;
            } else {
                iconFilePath = icoPath;
            }
            ConsolePrintln(L"icon" + m_id + iconFilePath);
            HBITMAP iconBitmap = nullptr;
            if (!iconFilePath.empty()) {
                if (EndsWithAnyIgnoreCase(iconFilePath, {L".ico", L".png", L".jpeg", L".jpg", L".bmp", L".tiff", L".gif"})) {
                    iconBitmap = LoadPngAsHBITMAP(iconFilePath.c_str(), 48, 48);
                } else if (EndsWithIgnoreCase(iconFilePath, L".svg")) {
                    iconBitmap = LoadSvgAsHBITMAP(iconFilePath.c_str(), 48, 48);
                } else {
                    iconBitmap = GetIconFromPathAsBitmap(iconFilePath.c_str());
                }
            }
            // LogBitmapInfo(iconBitmap);
            pluginIconMap.emplace(m_id, UniqueHBitmap(iconBitmap));
        }
    }

    const std::wstring& GetDir() const {
        return m_dir;
    }

    const std::wstring& GetId() const {
        return m_id;
    }

    const std::wstring& GetName() const {
        return m_name;
    }

    const std::wstring& GetEntry() const {
        return m_entry;
    }

    const std::vector<std::wstring>& GetTriggerKeywords() const {
        return m_triggerKeywords;
    }
};

class NodeJsScriptManager {
private:
    std::vector<std::shared_ptr<NodeJsScript>> m_scripts;

public:
    void ScanPlugins(const std::wstring& scriptsDir) {
        m_scripts.clear();
        if (!std::filesystem::exists(scriptsDir)) return;

        for (const auto& entry : std::filesystem::directory_iterator(scriptsDir)) {
            if (entry.is_directory()) {
                std::wstring pluginJsonPath = entry.path().wstring() + L"\\plugin.json";
                if (std::filesystem::exists(pluginJsonPath)) {
                    try {
                        std::string content = ReadUtf8File(pluginJsonPath);
                        auto j = nlohmann::json::parse(content);
                        // Flow launcher plugins might not have "Runtime": "nodejs", 
                        // but they have Language: "javascript" or "typescript"
                        bool isNode = false;
                        if (j.contains("Runtime") && j["Runtime"] == "nodejs") isNode = true;
                        else if (j.contains("Language") && (j["Language"] == "javascript" || j["Language"] == "typescript")) isNode = true;

                        if (isNode) {
                            m_scripts.push_back(std::make_shared<NodeJsScript>(entry.path().wstring(), j));
                            ConsolePrintln(L"NodeJsScriptManager", L"Found plugin: " + utf8_to_wide(j["Name"]));
                        }
                    } catch (...) {
                        // Ignore invalid plugins
                    }
                }
            }
        }
    }

    const std::vector<std::shared_ptr<NodeJsScript>>& GetAllScripts() const {
        return m_scripts;
    }

    std::shared_ptr<NodeJsScript> GetScriptById(const std::wstring& id) const {
        for (const auto& script : m_scripts) {
            if (script->GetId() == id) return script;
        }
        return nullptr;
    }
};
