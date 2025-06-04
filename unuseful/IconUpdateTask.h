#pragma once
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>

#include "../framework.h"
#include "../ListedRunnerPlugin.h"
// #include "RunCommandAction.cpp"

class IconUpdateTask {
public:
	IconUpdateTask(std::vector<std::shared_ptr<RunCommandAction>>& actionsRef);
	~IconUpdateTask();

	void Start();
	void Cancel();
	bool IsRunning() const;

private:
	void Run();

	std::thread worker;
	std::atomic<bool> cancelRequested;
	std::atomic<bool> running;
	std::vector<std::shared_ptr<RunCommandAction>>& actions;
	std::mutex iconMutex;
};
