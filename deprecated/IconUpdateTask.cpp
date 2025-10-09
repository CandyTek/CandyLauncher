//
// Created by Administrator on 2025/5/19.
//

#include "../IconUpdateTask.h"

#include "../framework.h"
// #include "ShellIconHelper.hpp" // 包含 GetIconFromPath()

IconUpdateTask::IconUpdateTask(std::vector<std::shared_ptr<RunCommandAction>>& actionsRef)
	: actions(actionsRef), cancelRequested(false), running(false) {}

IconUpdateTask::~IconUpdateTask() {
	Cancel();
	if (worker.joinable()) {
		worker.join();
	}
}

void IconUpdateTask::Start() {
	Cancel(); // 取消前一次任务
	cancelRequested = false;
	running = true;

	worker = std::thread(&IconUpdateTask::Run, this);
}

void IconUpdateTask::Cancel() {
	cancelRequested = true;
}

bool IconUpdateTask::IsRunning() const {
	return running;
}

static HICON GetFileIcon(const std::wstring& filePath, bool largeIcon = false) {
	SHFILEINFOW sfi;
	UINT flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES;
	flags |= (largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON);

	SHGetFileInfoW(filePath.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags);
	return sfi.hIcon;
}

static HBITMAP IconToBitmap(HICON hIcon) {
	if (!hIcon) return nullptr;

	ICONINFO iconInfo;
	GetIconInfo(hIcon, &iconInfo);

	return iconInfo.hbmColor;
}

static HBITMAP GetIconFromPath(const std::wstring& path) {
	std::wstring actualPath = path;

	if (path.size() > 4 && path.substr(path.size() - 4) == L".lnk") {
		actualPath = ListedRunnerPlugin::GetShortcutTarget(path);
	}

	HICON hIcon = GetFileIcon(actualPath, true);
	HBITMAP hBitmap = IconToBitmap(hIcon);
	DestroyIcon(hIcon);
	return hBitmap;
}


void IconUpdateTask::Run() {
	for (auto& action : actions) {
		if (cancelRequested) break;

		if (!action->IsUwpItem) {
			HBITMAP bmp = GetIconFromPath(action->GetTargetPath());
			{
				std::lock_guard<std::mutex> lock(iconMutex);
				action->SetIcon(bmp); // 你需要给 RunCommandAction 添加 SetIcon(HBITMAP)
			}
		}
	}

	running = false;
}

