#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <sqlite3.h>
#include <algorithm>
#include <memory>
#include "EverNoteAction.hpp"
#include "../../util/StringUtil.hpp"
#include "util/LogUtil.hpp"

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

// 主函数：解析 Evernote 数据库
inline void db_parse(sqlite3* db, std::vector<std::shared_ptr<BaseAction>>& allActions) {
	std::wcout << L"[EverNote] db_parse start" << std::endl;

	// 1. 读取 Notebook 表建立 ID -> 名称映射
	std::wcout << L"[EverNote] Reading Nodes_Notebook table..." << std::endl;
	std::unordered_map<std::string, std::string> notebookMap;  // id -> label
	{
		std::vector<std::vector<std::string>> result;
		if (!GetTable(db, "SELECT id, label FROM Nodes_Notebook;", result)) {
			Loge(L"EverNote", L"Error reading Nodes_Notebook table: ", sqlite3_errmsg(db));
			return;
		}
		std::wcout << L"[EverNote] Read " << result.size() << L" rows from Nodes_Notebook table" << std::endl;

		for (auto& row : result) {
			if (row.size() < 2) continue;
			notebookMap[row[0]] = row[1];  // id -> label
		}
	}

	// 2. 读取 Nodes_Note 表
	std::wcout << L"[EverNote] Reading Nodes_Note table..." << std::endl;
	{
		std::vector<std::vector<std::string>> result;
		// 查询：id, label, snippet, parent_Notebook_id, owner, shardId
		// WHERE deleted IS NULL 过滤已删除的笔记
		if (!GetTable(db, "SELECT id, label, snippet, parent_Notebook_id, owner, shardId FROM Nodes_Note WHERE deleted IS NULL;", result)) {
			std::wcerr << L"[EverNote] Error reading Nodes_Note table: " << sqlite3_errmsg(db) << std::endl;
			return;
		}
		std::wcout << L"[EverNote] Read " << result.size() << L" rows from Nodes_Note table" << std::endl;

		for (auto& row : result) {
			// row[0] = id, row[1] = label, row[2] = snippet, row[3] = parent_Notebook_id
			// row[4] = owner, row[5] = shardId
			if (row.size() < 6) continue;

			try {
				auto action = std::make_shared<EverNoteAction>();

				// 设置笔记 ID (GUID)
				action->noteId = utf8_to_wide(row[0]);

				// 设置标题 (label)
				std::string label = row[1].empty() ? "无标题" : row[1];
				action->title = utf8_to_wide(label);

				// 设置 userId 和 shardId
				// owner 是浮点数，需要转换为整数字符串
				if (!row[4].empty()) {
					try {
						double owner_double = std::stod(row[4]);
						long long owner_int = static_cast<long long>(owner_double);
						action->userId = std::to_wstring(owner_int);
					} catch (...) {
						action->userId = L"";
					}
				}
				action->shardId = utf8_to_wide(row[5]);

				// 设置副标题 (snippet 片段 + 笔记本名称)
				std::string notebook_id = row[3];
				std::string notebook_name = "未知笔记本";
				if (!notebook_id.empty() && notebookMap.count(notebook_id)) {
					notebook_name = notebookMap[notebook_id];
				}
				action->notebookName = utf8_to_wide(notebook_name);

				// 副标题显示：笔记本 | 摘要
				std::string snippet = row[2].empty() ? "" : row[2];
				if (snippet.length() > 100) {
					snippet = snippet.substr(0, 100) + "...";
				}
				action->subTitle = action->notebookName;
				if (!snippet.empty()) {
					action->subTitle += L" | " + utf8_to_wide(snippet);
				}

				// 设置图标（可以使用 Evernote 的默认图标或自定义）
				action->iconFilePath = L"";  // 暂时留空
				action->iconFilePathIndex = -1;

				// 设置匹配文本（标题 + 笔记本名称用于搜索）
				std::wstring searchText = action->title + L" " + action->notebookName;
				action->matchText = m_host->GetTheProcessedMatchingText(searchText);

				// 添加到 allActions
				allActions.push_back(action);
			} catch (const std::exception& e) {
				std::wcerr << L"[EverNote] Error parsing note row: " << e.what()
				          << L" | row size: " << row.size() << std::endl;
				continue;
			}
		}
	}

	std::wcout << L"[EverNote] db_parse complete, created " << allActions.size() << L" actions" << std::endl;
}
