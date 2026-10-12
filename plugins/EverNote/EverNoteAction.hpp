#pragma once
#include <string>
#include "EverNotePluginData.hpp"

class EverNoteAction final : public candy::ActionBase {
public:
	std::wstring noteId;  // Evernote 笔记 GUID
	std::wstring notebookName;  // 笔记本名称
	std::wstring userId;  // 用户 ID (owner)
	std::wstring shardId;  // Shard ID

	// 构造 Evernote In-App Note Link:
	// evernote:///view/[userId]/[shardId]/[noteGuid]/[noteGuid]/
	// 注意：noteGuid 需要重复两次
	std::wstring GetNoteUrl() const {
		return L"evernote:///view/" + userId + L"/" + shardId + L"/" + noteId + L"/" + noteId + L"/";
	}
};
