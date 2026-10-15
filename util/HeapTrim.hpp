#pragma once
#include <windows.h>

#include "LogUtil.hpp"

// 把进程里所有 NT 堆缓存着的空闲页还给系统（Windows 8.1+ 的 HeapOptimizeResources，Win7 上调用失败即忽略）。
// 插件刷新会先释放整批旧条目再发布新条目，低碎片堆会把释放出来的子段留作缓存，实测索引完成后堆里有几 MB
// 已提交但空闲的内存；在列表重建完成后延迟几秒调用一次（主窗口 TIMER_HEAP_TRIM），对查询速度没有影响。
inline void TrimProcessHeaps() {
	HEAP_OPTIMIZE_RESOURCES_INFORMATION info{};
	info.Version = HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION;
	if (!HeapSetInformation(nullptr, HeapOptimizeResources, &info, sizeof(info))) {
		// Win7 没有这个选项（ERROR_INVALID_PARAMETER），不是错误
		static bool logged = false;
		if (!logged) {
			logged = true;
			Logi(L"HeapTrim", L"HeapOptimizeResources unavailable, error ", GetLastError());
		}
	}
}
