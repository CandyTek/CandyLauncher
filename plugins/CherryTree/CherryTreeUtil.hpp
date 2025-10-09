#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <sqlite3.h>
#include <algorithm>
#include <memory>
#include "CherryTreeAction.hpp"
#include "../../util/StringUtil.hpp"
#include "util/LogUtil.hpp"

// 结构体定义
struct NodeRow {
	int id;
	std::string name;
	std::string tags;  // 重命名为 tags，更清晰
};

struct NodePath {
	std::vector<int> father_ids;
	std::string path;
	std::string path_file;
};

// 执行 SQL 并读取结果
inline bool GetTable(sqlite3* db, const std::string& sql, std::vector<std::vector<std::string>>& rows) {
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

	int colCount = sqlite3_column_count(stmt);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		std::vector<std::string> row;
		for (int i = 0; i < colCount; ++i) {
			const unsigned char* text = sqlite3_column_text(stmt, i);
			row.push_back(text ? reinterpret_cast<const char*>(text) : "");
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
	std::unordered_map<int, NodePath> id_path;

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
		if (!GetTable(db, "SELECT node_id, name, tags FROM node;", result)) {
			std::wcerr << L"[CherryTree] Error reading node table: " << sqlite3_errmsg(db) << std::endl;
			return;
		}
		std::wcout << L"[CherryTree] Read " << result.size() << L" rows from node table" << std::endl;

		for (auto& row : result) {
			// 只查询需要的字段：node_id, name, tags
			// row[0] = node_id, row[1] = name, row[2] = tags
			if (row.size() < 3) continue;

			try {
				NodeRow n;
				n.id = std::stoi(row[0]);      // node_id
				n.name = row[1];               // name
				n.tags = row[2];               // tags
				map_node[n.id] = n;
			} catch (const std::exception& e) {
				std::wcerr << L"[CherryTree] Error parsing node row: " << e.what()
				          << L" | row size: " << row.size() << std::endl;
				continue;
			}
		}
	}

	// 3. 生成路径映射
	std::wcout << L"[CherryTree] Generating path mappings for " << map_father.size() << L" nodes..." << std::endl;
	for (auto& [child, father] : map_father) {
		if (!map_node.count(child)) continue;

		NodePath np;
		std::string path_string = map_node[child].name;
		std::string path_string_file = path_string;

		// 屏蔽节点检查
		if (map_node[child].tags.find("屏蔽") != std::string::npos) {
			ConsolePrintln(L"db_parse","屏蔽节点 " + std::to_string(child) + " " + map_node[child].name);
			continue;
		}

		int v = father;
		while (true) {
			if (v != 0 && map_node.count(v)) {
				if (map_node[v].tags.find("屏蔽") != std::string::npos) {
					ConsolePrintln(L"db_parse","屏蔽节点 " + std::to_string(child) + " " + map_node[child].name +
						" 父节点有 屏蔽 标签 " + std::to_string(v));
					np.father_ids.clear();
					break;
				}
			}

			if (map_father.count(v)) {
				path_string = map_node[v].name + " - " + path_string;
				path_string_file = map_node[v].name + "--" + path_string_file;
				np.father_ids.push_back(v);
				v = map_father[v];
			} else {
				np.father_ids.push_back(0);
				np.path = "[" + std::to_string(child) + "]" + path_string;

				std::replace(path_string_file.begin(), path_string_file.end(), ' ', '_');
				std::replace(path_string_file.begin(), path_string_file.end(), '/', '-');

				np.path_file = path_string_file + "_" + std::to_string(child);
				break;
			}
		}

		if (!np.path.empty()) id_path[child] = np;
	}

	// 4. 构造 CherryTreeAction 对象并添加到 allActions
	std::wcout << L"[CherryTree] Creating " << id_path.size() << L" CherryTreeAction objects..." << std::endl;
	for (auto& [id, np] : id_path) {
		auto action = std::make_shared<CherryTreeAction>();

		// 设置标题为节点路径
		action->title = utf8_to_wide(np.path);

		// 设置副标题为文件路径
		action->subTitle = utf8_to_wide(np.path_file);

		// 设置 nodeId 用于打开 CherryTree 文件定位到该节点
		action->nodeId = id;

		// 设置 URL（可以后续用于打开 CherryTree 文件定位到该节点）
		action->url = L"cherrytree://node/" + std::to_wstring(id);

		// 设置图标（可以使用 CherryTree 的默认图标或自定义）
		action->iconFilePath = L""; // 暂时留空，可以后续设置
		action->iconFilePathIndex = -1;
		action->matchText = m_host->GetTheProcessedMatchingText(utf8_to_wide(np.path));

		// 添加到 allActions
		allActions.push_back(action);
	}
	std::wcout << L"[CherryTree] db_parse complete, created " << allActions.size() << L" actions" << std::endl;
}
