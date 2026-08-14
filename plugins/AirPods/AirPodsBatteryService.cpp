#include "AirPodsBatteryService.hpp"

#include <algorithm>
#include <condition_variable>
#include <cwctype>
#include <vector>

#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Enumeration;

namespace {
	constexpr uint16_t AppleVendorId = 0x004C;

	std::wstring ModelName(const uint16_t modelId) {
		switch (modelId) {
		case 0x2002: return L"AirPods (1st generation)";
		case 0x200F: return L"AirPods (2nd generation)";
		case 0x2013: return L"AirPods (3rd generation)";
		case 0x2019: return L"AirPods 4";
		case 0x201B: return L"AirPods 4 (ANC)";
		case 0x200E: return L"AirPods Pro";
		case 0x2014: return L"AirPods Pro (2nd generation)";
		case 0x2024: return L"AirPods Pro 2 (USB-C)";
		case 0x2027: return L"AirPods Pro 3";
		case 0x200A: return L"AirPods Max";
		case 0x2012: return L"Beats Fit Pro";
		default: return L"Apple headphones";
		}
	}

	bool IsSupportedModel(const uint16_t modelId) {
		return ModelName(modelId) != L"Apple headphones";
	}

	bool LooksLikeAppleHeadphones(std::wstring value) {
		std::transform(value.begin(), value.end(), value.begin(), towlower);
		return value.find(L"airpods") != std::wstring::npos ||
			value.find(L"beats") != std::wstring::npos;
	}

	std::optional<int> DecodeBattery(const uint8_t value) {
		if (value > 10) return std::nullopt;
		return static_cast<int>(value) * 10;
	}

	void SetError(AirPodsBatteryState& state, const hresult_error& error) {
		state.error = error.message().c_str();
	}
}

std::mutex stopMutex;
std::condition_variable stopCv;

AirPodsBatteryService::~AirPodsBatteryService() {
	Stop();
}

void AirPodsBatteryService::Start() {
	if (worker.joinable()) return;

	stopRequested = false;
	worker = std::thread(&AirPodsBatteryService::Worker, this);
}

void AirPodsBatteryService::Stop() {
	stopRequested = true;
	stopCv.notify_all();

	if (worker.joinable()) {
		worker.join();
	}

	std::lock_guard<std::mutex> lock(mutex);
	state = {};
	connectedModelId = 0;
}

AirPodsBatteryState AirPodsBatteryService::GetState() const {
	std::lock_guard<std::mutex> lock(mutex);
	auto result = state;
	if (result.updatedAt != std::chrono::steady_clock::time_point{} &&
		std::chrono::steady_clock::now() - result.updatedAt > std::chrono::seconds(8)) {
		result.left.reset();
		result.right.reset();
		result.caseBox.reset();
		result.leftCharging = false;
		result.rightCharging = false;
		result.caseCharging = false;
	}
	return result;
}

void AirPodsBatteryService::Worker() {
	init_apartment(apartment_type::multi_threaded);

	BluetoothLEAdvertisementWatcher watcher;
	event_token receivedToken{};
	event_token stoppedToken{};

	try {
		watcher.ScanningMode(BluetoothLEScanningMode::Active);

		receivedToken = watcher.Received([this](
			const BluetoothLEAdvertisementWatcher&,
			const BluetoothLEAdvertisementReceivedEventArgs& args) {
			for (const auto& manufacturerData : args.Advertisement().ManufacturerData()) {
				if (manufacturerData.CompanyId() != AppleVendorId) continue;

				const auto buffer = manufacturerData.Data();
				if (buffer.Length() != 27) continue;

				const auto* bytes = buffer.data();
				if (bytes[0] != 0x07 || bytes[1] != 25) continue;

				const uint16_t modelId =
					static_cast<uint16_t>(bytes[3]) |
					(static_cast<uint16_t>(bytes[4]) << 8);
				if (!IsSupportedModel(modelId)) continue;

				std::lock_guard<std::mutex> lock(mutex);
				if (!state.connected) continue;
				if (connectedModelId != 0 && connectedModelId != modelId) continue;

				const bool broadcastFromLeft = (bytes[5] & 0x20) != 0;
				const uint8_t currentBattery = bytes[6] & 0x0F;
				const uint8_t otherBattery = (bytes[6] >> 4) & 0x0F;

				state.modelName = ModelName(modelId);
				state.left = DecodeBattery(broadcastFromLeft ? currentBattery : otherBattery);
				state.right = DecodeBattery(broadcastFromLeft ? otherBattery : currentBattery);
				state.caseBox = DecodeBattery(bytes[7] & 0x0F);
				state.leftCharging = (bytes[7] & (broadcastFromLeft ? 0x10 : 0x20)) != 0;
				state.rightCharging = (bytes[7] & (broadcastFromLeft ? 0x20 : 0x10)) != 0;
				state.caseCharging = (bytes[7] & 0x40) != 0;
				state.updatedAt = std::chrono::steady_clock::now();
				state.error.clear();
			}
		});

		stoppedToken = watcher.Stopped([this](
			const BluetoothLEAdvertisementWatcher&,
			const BluetoothLEAdvertisementWatcherStoppedEventArgs& args) {
			std::lock_guard<std::mutex> lock(mutex);
			state.scannerAvailable = false;
			state.error = L"Bluetooth scanner stopped (error " +
				std::to_wstring(static_cast<uint32_t>(args.Error())) + L")";
		});

		watcher.Start();
		{
			std::lock_guard<std::mutex> lock(mutex);
			state.scannerAvailable = true;
			state.error.clear();
		}

		while (!stopRequested) {
			try {
				const auto selector =
					BluetoothDevice::GetDeviceSelectorFromConnectionStatus(
						BluetoothConnectionStatus::Connected);

				const param::async_iterable<hstring> properties{
					L"System.DeviceInterface.Bluetooth.VendorId",
					L"System.DeviceInterface.Bluetooth.ProductId"
				};

				const auto devices =
					DeviceInformation::FindAllAsync(selector, properties).get();

				bool found = false;
				std::wstring deviceName;
				uint16_t productId = 0;

				for (const auto& deviceInfo : devices) {
					if (stopRequested.load()) {
						break;
					}

					const auto deviceProperties = deviceInfo.Properties();

					const auto vendorValue = deviceProperties.TryLookup(
						L"System.DeviceInterface.Bluetooth.VendorId");

					const auto productValue = deviceProperties.TryLookup(
						L"System.DeviceInterface.Bluetooth.ProductId");

					const auto vendorId =
						unbox_value_or<uint16_t>(vendorValue, 0);

					const auto candidateProductId =
						unbox_value_or<uint16_t>(productValue, 0);

					const std::wstring candidateName =
						deviceInfo.Name().c_str();

					if ((vendorId == AppleVendorId &&
						 IsSupportedModel(candidateProductId)) ||
						LooksLikeAppleHeadphones(candidateName)) {

						found = true;
						deviceName = candidateName;
						productId = candidateProductId;
						break;
						}
				}

				std::lock_guard<std::mutex> lock(mutex);
				const bool deviceChanged =
					state.connected && found &&
					(connectedModelId != productId || state.deviceName != deviceName);
				if (deviceChanged) {
					state.modelName.clear();
					state.left.reset();
					state.right.reset();
					state.caseBox.reset();
					state.leftCharging = false;
					state.rightCharging = false;
					state.caseCharging = false;
					state.updatedAt = {};
				}

				state.connected = found;
				connectedModelId = productId;
				if (found) {
					state.deviceName = deviceName;
					// A live Apple advertisement is more authoritative than the
					// Windows device-interface product metadata.
					if (productId != 0 &&
						state.updatedAt == std::chrono::steady_clock::time_point{}) {
						state.modelName = ModelName(productId);
					}
				} else {
					state.deviceName.clear();
					state.modelName.clear();
					state.left.reset();
					state.right.reset();
					state.caseBox.reset();
					state.leftCharging = false;
					state.rightCharging = false;
					state.caseCharging = false;
					state.updatedAt = {};
				}
			} catch (const hresult_error& error) {
				std::lock_guard<std::mutex> lock(mutex);
				state.connected = false;
				connectedModelId = 0;
				state.deviceName.clear();
				state.modelName.clear();
				state.left.reset();
				state.right.reset();
				state.caseBox.reset();
				state.leftCharging = false;
				state.rightCharging = false;
				state.caseCharging = false;
				state.updatedAt = {};
				SetError(state, error);
			}

			std::unique_lock<std::mutex> lock(stopMutex);
			stopCv.wait_for(lock,std::chrono::seconds(1),[this] {
				return stopRequested.load();
			});
		}

		watcher.Stop();
		watcher.Received(receivedToken);
		watcher.Stopped(stoppedToken);
	} catch (const hresult_error& error) {
		std::lock_guard<std::mutex> lock(mutex);
		state.scannerAvailable = false;
		SetError(state, error);
	}

	uninit_apartment();
}
