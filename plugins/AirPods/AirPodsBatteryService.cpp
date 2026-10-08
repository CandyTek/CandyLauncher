#include "AirPodsBatteryService.hpp"

#include <algorithm>
#include <cwctype>
#include <map>
#include <memory>
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
	constexpr auto BatteryStaleAfter = std::chrono::seconds(8);
	// Re-reads the connection property from the device store (no radio traffic). The cached
	// BluetoothDevice::ConnectionStatus and watcher updates are not reliable for audio devices.
	constexpr auto ConnectionRecheckInterval = std::chrono::seconds(2);
	const wchar_t* const VendorIdProperty = L"System.DeviceInterface.Bluetooth.VendorId";
	const wchar_t* const ProductIdProperty = L"System.DeviceInterface.Bluetooth.ProductId";
	const wchar_t* const IsConnectedProperty = L"System.Devices.Aep.IsConnected";

	std::wstring ModelName(const uint16_t modelId) {
		switch (modelId) {
		case 0x2002: return L"AirPods 1";
		case 0x200F: return L"AirPods 2";
		case 0x2013: return L"AirPods 3";
		case 0x2019: return L"AirPods 4";
		case 0x201B: return L"AirPods 4 ANC";
		case 0x200E: return L"AirPods Pro";
		case 0x2014: return L"AirPods Pro 2";
		case 0x2024: return L"AirPods Pro 2 (C)";
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

	bool IsStale(const AirPodsBatteryState& state, const std::chrono::steady_clock::time_point now) {
		return state.updatedAt == std::chrono::steady_clock::time_point{} ||
			now - state.updatedAt > BatteryStaleAfter;
	}

	uint16_t ProductIdOf(const DeviceInformation& info) {
		return unbox_value_or<uint16_t>(info.Properties().TryLookup(ProductIdProperty), 0);
	}

	bool IsAppleHeadphones(const DeviceInformation& info) {
		const auto vendorId = unbox_value_or<uint16_t>(info.Properties().TryLookup(VendorIdProperty), 0);
		return (vendorId == AppleVendorId && IsSupportedModel(ProductIdOf(info))) ||
			LooksLikeAppleHeadphones(info.Name().c_str());
	}

	// One paired Bluetooth device. Apple headphones additionally get a BluetoothDevice
	// whose ConnectionStatusChanged event reports connect/disconnect immediately.
	struct PairedDevice {
		DeviceInformation info{nullptr};
		BluetoothDevice device{nullptr};
		event_token connectionToken{};
		bool attaching = false;
		// Latest IsConnected read from the device store; takes precedence when known.
		std::optional<bool> connected;

		void Release() {
			if (!device) return;
			try {
				device.ConnectionStatusChanged(connectionToken);
				device.Close();
			} catch (const hresult_error&) {
			}
			device = nullptr;
		}

		bool IsConnected() const {
			if (connected) return *connected;
			if (device) {
				try {
					return device.ConnectionStatus() == BluetoothConnectionStatus::Connected;
				} catch (const hresult_error&) {
				}
			}
			return unbox_value_or<bool>(info.Properties().TryLookup(IsConnectedProperty), false);
		}
	};

	// Shared with the WinRT event handlers, which may still be in flight after the
	// worker has returned, so the handlers keep it alive themselves.
	struct WatchSession {
		std::atomic<bool> closed{false};
		std::map<std::wstring, PairedDevice> devices;
	};
}

AirPodsBatteryService::~AirPodsBatteryService() {
	Stop();
}

void AirPodsBatteryService::SetOnChanged(std::function<void()> callback) {
	std::lock_guard<std::mutex> lock(mutex);
	onChanged = std::move(callback);
}

void AirPodsBatteryService::Start() {
	if (worker.joinable()) return;

	stopRequested = false;
	recheckRequested = false;
	{
		std::lock_guard<std::mutex> lock(mutex);
		state.ready = false;
		state.error.clear();
	}
	worker = std::thread(&AirPodsBatteryService::Worker, this);
}

void AirPodsBatteryService::Stop() {
	{
		std::lock_guard<std::mutex> lock(stopMutex);
		stopRequested = true;
	}
	stopCv.notify_all();

	if (worker.joinable()) {
		worker.join();
	}

	// Keep the last known device/battery so the next activation can show something
	// immediately; GetState() hides the battery once it becomes stale.
	std::lock_guard<std::mutex> lock(mutex);
	state.ready = false;
	state.scannerAvailable = false;
}

AirPodsBatteryState AirPodsBatteryService::GetState() const {
	std::lock_guard<std::mutex> lock(mutex);
	auto result = state;
	if (result.updatedAt != std::chrono::steady_clock::time_point{} &&
		IsStale(result, std::chrono::steady_clock::now())) {
		result.left.reset();
		result.right.reset();
		result.caseBox.reset();
		result.leftCharging = false;
		result.rightCharging = false;
		result.caseCharging = false;
	}
	return result;
}

void AirPodsBatteryService::NotifyChanged() {
	std::function<void()> callback;
	{
		std::lock_guard<std::mutex> lock(mutex);
		callback = onChanged;
	}
	if (callback) callback();
}

void AirPodsBatteryService::ClearBatteryLocked() {
	state.modelName.clear();
	state.left.reset();
	state.right.reset();
	state.caseBox.reset();
	state.leftCharging = false;
	state.rightCharging = false;
	state.caseCharging = false;
	state.updatedAt = {};
}

void AirPodsBatteryService::Worker() {
	init_apartment(apartment_type::multi_threaded);

	const auto session = std::make_shared<WatchSession>();

	// Picks the connected Apple headphones out of the paired devices. Caller holds `mutex`.
	const auto recomputeConnectedLocked = [this, session] {
		bool found = false;
		std::wstring deviceName;
		uint16_t productId = 0;

		for (const auto& [id, paired] : session->devices) {
			if (!IsAppleHeadphones(paired.info) || !paired.IsConnected()) continue;
			found = true;
			deviceName = paired.info.Name().c_str();
			productId = ProductIdOf(paired.info);
			break;
		}

		const bool changed = state.connected != found ||
			(found && (connectedModelId != productId || state.deviceName != deviceName));
		if (!changed) return false;

		// Disconnected or switched to another headset: old battery values no longer apply.
		ClearBatteryLocked();
		state.connected = found;
		connectedModelId = productId;
		state.deviceName = found ? deviceName : std::wstring{};
		if (found && productId != 0) {
			state.modelName = ModelName(productId);
		}
		return true;
	};

	const auto refreshConnection = [this, session, recomputeConnectedLocked] {
		if (session->closed) return;
		bool changed;
		{
			std::lock_guard<std::mutex> lock(mutex);
			changed = state.ready && recomputeConnectedLocked();
		}
		if (changed) NotifyChanged();
	};

	// Wakes the worker so it re-reads the connection state right away.
	const auto requestRecheck = [this, session] {
		if (session->closed) return;
		{
			std::lock_guard<std::mutex> lock(stopMutex);
			recheckRequested = true;
		}
		stopCv.notify_all();
	};

	// Reads IsConnected fresh from the device store for every Apple headphones entry.
	// Runs on the worker thread without holding `mutex`.
	const auto pollConnection = [this, session, refreshConnection] {
		std::vector<std::wstring> ids;
		{
			std::lock_guard<std::mutex> lock(mutex);
			for (const auto& [id, paired] : session->devices) {
				if (IsAppleHeadphones(paired.info)) ids.push_back(id);
			}
		}
		if (ids.empty()) return;

		std::vector<std::pair<std::wstring, bool>> results;
		for (const auto& id : ids) {
			if (session->closed) return;
			try {
				const auto info = DeviceInformation::CreateFromIdAsync(
					id, std::vector<hstring>{hstring{IsConnectedProperty}},
					DeviceInformationKind::AssociationEndpoint).get();
				const auto value = info.Properties().TryLookup(IsConnectedProperty);
				if (value) results.emplace_back(id, unbox_value_or<bool>(value, false));
			} catch (const hresult_error&) {
			}
		}

		{
			std::lock_guard<std::mutex> lock(mutex);
			for (const auto& [id, isConnected] : results) {
				const auto it = session->devices.find(id);
				if (it != session->devices.end()) it->second.connected = isConnected;
			}
		}
		refreshConnection();
	};

	// Opens a BluetoothDevice for Apple headphones so connection changes arrive as events.
	// Opening the object does not connect to the device. Runs without holding `mutex`.
	const auto attachDevice = [this, session, requestRecheck](const std::wstring& id) {
		{
			std::lock_guard<std::mutex> lock(mutex);
			const auto it = session->devices.find(id);
			if (it == session->devices.end() || it->second.device || it->second.attaching) return;
			if (!IsAppleHeadphones(it->second.info)) return;
			it->second.attaching = true;
		}

		BluetoothDevice device{nullptr};
		try {
			device = BluetoothDevice::FromIdAsync(id).get();
		} catch (const hresult_error&) {
		}

		{
			std::lock_guard<std::mutex> lock(mutex);
			const auto it = session->devices.find(id);
			if (it != session->devices.end()) it->second.attaching = false;
			if (!device) return;
			if (session->closed || it == session->devices.end()) {
				device.Close();
				return;
			}
			it->second.device = device;
			it->second.connectionToken = device.ConnectionStatusChanged(
				[requestRecheck](const BluetoothDevice&, const Windows::Foundation::IInspectable&) {
					requestRecheck();
				});
		}
		requestRecheck();
	};

	BluetoothLEAdvertisementWatcher adWatcher{nullptr};
	DeviceWatcher deviceWatcher{nullptr};
	event_token receivedToken{};
	event_token adStoppedToken{};
	event_token addedToken{};
	event_token updatedToken{};
	event_token removedToken{};
	event_token enumeratedToken{};
	event_token deviceStoppedToken{};

	try {
		// Watch every paired classic Bluetooth device and track the connection ourselves:
		// a connection-status filter in the query is not re-evaluated when a device
		// connects or disconnects later, so it would miss both transitions.
		const auto selector = BluetoothDevice::GetDeviceSelectorFromPairingState(true);
		const std::vector<hstring> properties{
			hstring{IsConnectedProperty}, hstring{VendorIdProperty}, hstring{ProductIdProperty}
		};
		deviceWatcher = DeviceInformation::CreateWatcher(selector, properties);

		addedToken = deviceWatcher.Added([this, session, refreshConnection, attachDevice](
			const DeviceWatcher&, const DeviceInformation& info) {
			if (session->closed) return;
			const std::wstring id = info.Id().c_str();
			{
				std::lock_guard<std::mutex> lock(mutex);
				session->devices[id].info = info;
			}
			attachDevice(id);
			refreshConnection();
		});

		updatedToken = deviceWatcher.Updated([this, session, refreshConnection, attachDevice, requestRecheck](
			const DeviceWatcher&, const DeviceInformationUpdate& update) {
			if (session->closed) return;
			const std::wstring id = update.Id().c_str();
			{
				std::lock_guard<std::mutex> lock(mutex);
				const auto it = session->devices.find(id);
				if (it == session->devices.end()) return;
				it->second.info.Update(update);
				const auto value = update.Properties().TryLookup(IsConnectedProperty);
				if (value) it->second.connected = unbox_value_or<bool>(value, false);
			}
			// A rename may turn a device into (or out of) Apple headphones.
			attachDevice(id);
			refreshConnection();
			requestRecheck();
		});

		removedToken = deviceWatcher.Removed([this, session, refreshConnection](
			const DeviceWatcher&, const DeviceInformationUpdate& update) {
			if (session->closed) return;
			{
				std::lock_guard<std::mutex> lock(mutex);
				const auto it = session->devices.find(std::wstring(update.Id().c_str()));
				if (it == session->devices.end()) return;
				it->second.Release();
				session->devices.erase(it);
			}
			refreshConnection();
		});

		// The initial device set is only trusted once it is complete, so a device that was
		// disconnected while the plugin was inactive is noticed here.
		enumeratedToken = deviceWatcher.EnumerationCompleted([this, session, recomputeConnectedLocked](
			const DeviceWatcher&, const Windows::Foundation::IInspectable&) {
			if (session->closed) return;
			{
				std::lock_guard<std::mutex> lock(mutex);
				state.ready = true;
				recomputeConnectedLocked();
			}
			NotifyChanged();
		});

		deviceStoppedToken = deviceWatcher.Stopped([this, session](
			const DeviceWatcher& sender, const Windows::Foundation::IInspectable&) {
			if (session->closed || sender.Status() != DeviceWatcherStatus::Aborted) return;
			{
				std::lock_guard<std::mutex> lock(mutex);
				state.ready = true;
				state.error = L"Bluetooth device watcher aborted";
			}
			NotifyChanged();
		});

		// Battery: Apple broadcasts it in non-scannable advertisements, so passive
		// scanning is enough and the radio never sends scan requests.
		adWatcher = BluetoothLEAdvertisementWatcher();
		adWatcher.ScanningMode(BluetoothLEScanningMode::Passive);

		receivedToken = adWatcher.Received([this, session](
			const BluetoothLEAdvertisementWatcher&,
			const BluetoothLEAdvertisementReceivedEventArgs& args) {
			if (session->closed) return;
			bool changed = false;

			for (const auto& manufacturerData : args.Advertisement().ManufacturerData()) {
				if (manufacturerData.CompanyId() != AppleVendorId) continue;

				const auto buffer = manufacturerData.Data();
				if (buffer.Length() != 27) continue;

				// 0x07 / 25 is Apple's proximity pairing message, only sent by headphones.
				const auto* bytes = buffer.data();
				if (bytes[0] != 0x07 || bytes[1] != 25) continue;

				const uint16_t modelId =
					static_cast<uint16_t>(bytes[3]) |
					(static_cast<uint16_t>(bytes[4]) << 8);

				std::lock_guard<std::mutex> lock(mutex);
				if (!state.connected) continue;
				if (connectedModelId != 0 && connectedModelId != modelId) continue;

				const bool broadcastFromLeft = (bytes[5] & 0x20) != 0;
				const uint8_t currentBattery = bytes[6] & 0x0F;
				const uint8_t otherBattery = (bytes[6] >> 4) & 0x0F;

				const auto now = std::chrono::steady_clock::now();
				const auto modelName = ModelName(modelId);
				const auto left = DecodeBattery(broadcastFromLeft ? currentBattery : otherBattery);
				const auto right = DecodeBattery(broadcastFromLeft ? otherBattery : currentBattery);
				const auto caseBox = DecodeBattery(bytes[7] & 0x0F);
				const bool leftCharging = (bytes[7] & (broadcastFromLeft ? 0x10 : 0x20)) != 0;
				const bool rightCharging = (bytes[7] & (broadcastFromLeft ? 0x20 : 0x10)) != 0;
				const bool caseCharging = (bytes[7] & 0x40) != 0;

				// Advertisements arrive several times per second; only wake the UI on a visible change.
				changed = changed || IsStale(state, now) || !state.error.empty() ||
					state.modelName != modelName ||
					state.left != left || state.right != right || state.caseBox != caseBox ||
					state.leftCharging != leftCharging || state.rightCharging != rightCharging ||
					state.caseCharging != caseCharging;

				state.modelName = modelName;
				state.left = left;
				state.right = right;
				state.caseBox = caseBox;
				state.leftCharging = leftCharging;
				state.rightCharging = rightCharging;
				state.caseCharging = caseCharging;
				state.updatedAt = now;
				state.error.clear();
			}

			if (changed) NotifyChanged();
		});

		adStoppedToken = adWatcher.Stopped([this, session](
			const BluetoothLEAdvertisementWatcher&,
			const BluetoothLEAdvertisementWatcherStoppedEventArgs& args) {
			if (session->closed) return;
			{
				std::lock_guard<std::mutex> lock(mutex);
				state.scannerAvailable = false;
				state.error = L"Bluetooth scanner stopped (error " +
					std::to_wstring(static_cast<uint32_t>(args.Error())) + L")";
			}
			NotifyChanged();
		});

		deviceWatcher.Start();
		adWatcher.Start();
		{
			std::lock_guard<std::mutex> lock(mutex);
			state.scannerAvailable = true;
		}
	} catch (const hresult_error& error) {
		{
			std::lock_guard<std::mutex> lock(mutex);
			state.scannerAvailable = false;
			state.ready = true;
			state.error = error.message().c_str();
		}
		NotifyChanged();
	}

	while (true) {
		{
			std::unique_lock<std::mutex> lock(stopMutex);
			stopCv.wait_for(lock, ConnectionRecheckInterval, [this] {
				return stopRequested.load() || recheckRequested.load();
			});
			if (stopRequested) break;
			recheckRequested = false;
		}
		pollConnection();
	}

	// Release the Bluetooth stack as soon as the plugin is no longer in use.
	session->closed = true;
	try {
		if (adWatcher) {
			adWatcher.Received(receivedToken);
			adWatcher.Stopped(adStoppedToken);
			adWatcher.Stop();
		}
		if (deviceWatcher) {
			deviceWatcher.Added(addedToken);
			deviceWatcher.Updated(updatedToken);
			deviceWatcher.Removed(removedToken);
			deviceWatcher.EnumerationCompleted(enumeratedToken);
			deviceWatcher.Stopped(deviceStoppedToken);
			const auto status = deviceWatcher.Status();
			if (status == DeviceWatcherStatus::Started ||
				status == DeviceWatcherStatus::EnumerationCompleted) {
				deviceWatcher.Stop();
			}
		}
	} catch (const hresult_error&) {
	}

	{
		std::lock_guard<std::mutex> lock(mutex);
		for (auto& [id, paired] : session->devices) {
			paired.Release();
		}
		session->devices.clear();
	}
	adWatcher = nullptr;
	deviceWatcher = nullptr;
	uninit_apartment();
}
