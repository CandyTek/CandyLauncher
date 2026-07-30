#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <sqlite3.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <unordered_set>
#include <pugixml.hpp>
#include "CherryTreeAction.hpp"
#include "../../util/StringUtil.hpp"
#include "util/BitmapUtil.hpp"
#include "util/LogUtil.hpp"

// 结构体定义
struct NodeRow {
	int id;
	std::string name;
	std::string tags;  // 重命名为 tags，更清晰
	std::string syntax;
	std::string text;
};

struct NodePath {
	std::vector<int> father_ids;
	std::string path;
	std::string path_file;
	std::string syntax;
};

inline void CreateActionsFromNodes(
	const std::unordered_map<int, int>& map_father,
	const std::unordered_map<int, NodeRow>& map_node,
	std::vector<std::shared_ptr<BaseAction>>& allActions) {
	std::unordered_map<int, NodePath> id_path;

	std::wcout << L"[CherryTree] Generating path mappings for " << map_father.size() << L" nodes..." << std::endl;
	for (const auto& [child, father] : map_father) {
		auto childNode = map_node.find(child);
		if (childNode == map_node.end()) continue;

		NodePath np;
		std::string path_string = childNode->second.name;
		std::string path_string_file = path_string;

		if (childNode->second.tags.find(u8"屏蔽") != std::string::npos) {
			ConsolePrintln(L"db_parse", "屏蔽节点 " + std::to_string(child) + " " + childNode->second.name);
			continue;
		}

		int v = father;
		std::unordered_set<int> visited;
		visited.insert(child);
		while (true) {
			if (!visited.insert(v).second) {
				ConsolePrintln(L"db_parse", "节点层级存在循环 " + std::to_string(child));
				break;
			}

			if (v != 0) {
				auto parentNode = map_node.find(v);
				if (parentNode == map_node.end()) {
					ConsolePrintln(L"db_parse", "找不到父节点 " + std::to_string(v));
					break;
				}
				if (parentNode->second.tags.find(u8"屏蔽") != std::string::npos) {
					ConsolePrintln(L"db_parse", "屏蔽节点 " + std::to_string(child) + " " + childNode->second.name +
						" 父节点有 屏蔽 标签 " + std::to_string(v));
					break;
				}
			}

			auto parent = map_father.find(v);
			if (parent != map_father.end()) {
				const auto& parentNode = map_node.at(v);
				path_string = parentNode.name + " - " + path_string;
				path_string_file = parentNode.name + "--" + path_string_file;
				np.father_ids.push_back(v);
				v = parent->second;
			} else {
				np.father_ids.push_back(0);
				np.path = "[" + std::to_string(child) + "]" + path_string;
				np.syntax = childNode->second.syntax;

				std::replace(path_string_file.begin(), path_string_file.end(), ' ', '_');
				std::replace(path_string_file.begin(), path_string_file.end(), '/', '-');
				std::replace(path_string_file.begin(), path_string_file.end(), '\\', '-');
				np.path_file = path_string_file + "_" + std::to_string(child);
				break;
			}
		}

		if (!np.path.empty()) id_path[child] = np;
	}

	std::wcout << L"[CherryTree] Creating " << id_path.size() << L" CherryTreeAction objects..." << std::endl;
	for (const auto& [id, np] : id_path) {
		auto action = std::make_shared<CherryTreeAction>();
		action->title = utf8_to_wide(np.path);
		action->subTitle = utf8_to_wide(np.path_file);
		action->nodeId = id;
		action->url = L"cherrytree://node/" + std::to_wstring(id);
		action->text = utf8_to_wide(map_node.at(id).text);
		action->iconFilePathIndex = GetSysImageIndex(ICON_FOLDER_PATH + utf8_to_wide(np.syntax) + L".ico");
		if (action->iconFilePathIndex == 0) {
			ConsolePrintln(L"icon", ICON_FOLDER_PATH + utf8_to_wide(np.syntax) + L".ico");
		}
		action->matchText = m_host->GetTheProcessedMatchingText(utf8_to_wide(np.path));
		allActions.push_back(action);
	}
}

// 执行 SQL 并读取结果
inline bool GetTable(sqlite3* db, const std::string& sql, std::vector<std::vector<std::string>>& rows) {
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

	int colCount = sqlite3_column_count(stmt);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		std::vector<std::string> row;
		for (int i = 0; i < colCount; ++i) {
			const unsigned char* text = sqlite3_column_text(stmt, i);
			row.emplace_back(text ? reinterpret_cast<const char*>(text) : "");
		}
		rows.push_back(row);
	}

	sqlite3_finalize(stmt);
	return true;
}

// 主函数：解析数据库
inline void db_parse(sqlite3* db, std::vector<std::shared_ptr<BaseAction>>& allActions) {
	std::wcout << L"[CherryTree] db_parse start" << std::endl;
	std::unordered_map<int, int> map_father; // 子 -> 父
	std::unordered_map<int, NodeRow> map_node;

	// 1. 读取 children 表
	std::wcout << L"[CherryTree] Reading children table..." << std::endl;
	{
		std::vector<std::vector<std::string>> result;
		// children 表结构：node_id, father_id, sequence, master_id
		// 只查询需要的字段
		if (!GetTable(db, "SELECT node_id, father_id FROM children;", result)) {
			Loge(L"CherryTree", L"Error reading children table: " , sqlite3_errmsg(db) );
			return;
		}
		std::wcout << L"[CherryTree] Read " << result.size() << L" rows from children table" << std::endl;

		for (auto& row : result) {
			// row[0] = node_id (子节点ID), row[1] = father_id (父节点ID)
			if (row.size() < 2) continue;

			try {
				int child_id = std::stoi(row[0]);
				int father_id = std::stoi(row[1]);
				map_father[child_id] = father_id;
			} catch (const std::exception& e) {
				std::wcerr << L"[CherryTree] Error parsing children row: " << e.what()
				          << L" | row size: " << row.size() << std::endl;
				continue;
			}
		}
	}

	// 2. 读取 node 表
	std::wcout << L"[CherryTree] Reading node table..." << std::endl;
	{
		std::vector<std::vector<std::string>> result;
		if (!GetTable(db, "SELECT node_id, name, tags, syntax, txt FROM node;", result)) {
			std::wcerr << L"[CherryTree] Error reading node table: " << sqlite3_errmsg(db) << std::endl;
			return;
		}
		std::wcout << L"[CherryTree] Read " << result.size() << L" rows from node table" << std::endl;

		for (auto& row : result) {
			// row: node_id, name, tags, syntax, txt
			if (row.size() < 5) continue;

			try {
				NodeRow n;
				n.id = std::stoi(row[0]);      // node_id
				n.name = row[1];               // name
				n.tags = row[2];               // tags
				n.syntax = row[3];
				n.text = row[4];
				map_node[n.id] = n;
			} catch (const std::exception& e) {
				std::wcerr << L"[CherryTree] Error parsing node row: " << e.what()
				          << L" | row size: " << row.size() << std::endl;
				continue;
			}
		}
	}

	CreateActionsFromNodes(map_father, map_node, allActions);
	std::wcout << L"[CherryTree] db_parse complete, created " << allActions.size() << L" actions" << std::endl;
}

// 解析 CherryTree 多文件存储目录。每个节点目录包含 node.xml，父目录即父节点。
inline void folder_parse(
	const std::filesystem::path& directoryPath,
	std::vector<std::shared_ptr<BaseAction>>& allActions) {
	std::wcout << L"[CherryTree] folder_parse start" << std::endl;
	std::unordered_map<int, int> map_father;
	std::unordered_map<int, NodeRow> map_node;

	std::error_code pathError;
	std::filesystem::path rootPath = std::filesystem::weakly_canonical(directoryPath, pathError);
	if (pathError) {
		rootPath = directoryPath.lexically_normal();
		pathError.clear();
	}

	std::filesystem::recursive_directory_iterator iterator(
		rootPath,
		std::filesystem::directory_options::skip_permission_denied,
		pathError);
	const std::filesystem::recursive_directory_iterator end;
	if (pathError) {
		ConsolePrintln(L"folder_parse", L"无法读取目录: " + rootPath.wstring());
		return;
	}

	while (iterator != end) {
		const auto& entry = *iterator;
		std::error_code entryError;
		if (entry.is_regular_file(entryError) && entry.path().filename() == L"node.xml") {
			pugi::xml_document document;
			pugi::xml_parse_result result = document.load_file(entry.path().c_str());
			if (!result) {
				ConsolePrintln(L"folder_parse", L"XML 解析失败: " + entry.path().wstring() +
					L" (" + utf8_to_wide(result.description()) + L")");
			} else {
				pugi::xml_node node = document.child("cherrytree").child("node");
				if (!node) {
					ConsolePrintln(L"folder_parse", L"找不到 node 元素: " + entry.path().wstring());
				} else {
					int nodeId = node.attribute("unique_id").as_int();
					if (nodeId <= 0) {
						ConsolePrintln(L"folder_parse", L"无效的节点 ID: " + entry.path().wstring());
					} else {
						NodeRow row;
						row.id = nodeId;
						row.name = node.attribute("name").as_string();
						row.tags = node.attribute("tags").as_string();
						row.syntax = node.attribute("prog_lang").as_string("plain-text");
						for (pugi::xpath_node richTextNode : node.select_nodes(".//rich_text")) {
							std::string richText = richTextNode.node().text().as_string();
							if (!row.text.empty()) row.text += '\n';
							row.text += richText;
						}
						map_node[nodeId] = std::move(row);

						std::filesystem::path parentNodeDirectory = entry.path().parent_path().parent_path();
						if (parentNodeDirectory == rootPath) {
							map_father[nodeId] = 0;
						} else {
							try {
								map_father[nodeId] = std::stoi(parentNodeDirectory.filename().string());
							} catch (const std::exception&) {
								ConsolePrintln(L"folder_parse", L"无法确定父节点: " + entry.path().wstring());
								map_father[nodeId] = 0;
							}
						}
					}
				}
			}
		}

		iterator.increment(pathError);
		if (pathError) {
			ConsolePrintln(L"folder_parse", L"扫描目录时发生错误: " + utf8_to_wide(pathError.message()));
			pathError.clear();
		}
	}

	CreateActionsFromNodes(map_father, map_node, allActions);
	std::wcout << L"[CherryTree] folder_parse complete, created " << allActions.size() << L" actions" << std::endl;
}
