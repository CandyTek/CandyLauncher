#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
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
	// True once the connected-device enumeration has completed in the current session.
	bool ready = false;
	bool scannerAvailable = false;
	std::wstring error;
	std::chrono::steady_clock::time_point updatedAt{};
};

// Bluetooth is only touched between Start() and Stop(): a passive BLE advertisement
// watcher plus an event-driven device watcher. Nothing is polled and nothing stays
// registered with the Bluetooth stack after Stop().
class AirPodsBatteryService {
public:
	AirPodsBatteryService() = default;
	~AirPodsBatteryService();

	AirPodsBatteryService(const AirPodsBatteryService&) = delete;
	AirPodsBatteryService& operator=(const AirPodsBatteryService&) = delete;

	void Start();
	void Stop();
	AirPodsBatteryState GetState() const;
	// Invoked from a Bluetooth/thread-pool thread whenever the visible state changes.
	void SetOnChanged(std::function<void()> callback);

private:
	void Worker();
	void NotifyChanged();
	void ClearBatteryLocked();

	mutable std::mutex mutex;
	std::mutex stopMutex;
	std::condition_variable stopCv;
	std::thread worker;
	std::atomic<bool> stopRequested{false};
	std::atomic<bool> recheckRequested{false};
	AirPodsBatteryState state;
	uint16_t connectedModelId = 0;
	std::function<void()> onChanged;
};
