#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

struct AirPodsBatteryState {
	std::wstring deviceName;
	std::wstring modelName;
	std::optional<int> left;
	std::optional<int> right;
	std::optional<int> caseBox;
	bool leftCharging = false;
	bool rightCharging = false;
	bool caseCharging = false;
	bool connected = false;
	bool scannerAvailable = false;
	std::wstring error;
	std::chrono::steady_clock::time_point updatedAt{};
};

class AirPodsBatteryService {
public:
	AirPodsBatteryService() = default;
	~AirPodsBatteryService();

	AirPodsBatteryService(const AirPodsBatteryService&) = delete;
	AirPodsBatteryService& operator=(const AirPodsBatteryService&) = delete;

	void Start();
	void Stop();
	AirPodsBatteryState GetState() const;

private:
	void Worker();

	mutable std::mutex mutex;
	std::thread worker;
	std::atomic<bool> stopRequested{false};
	AirPodsBatteryState state;
	uint16_t connectedModelId = 0;
};
