#pragma once

#include <obs-module.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <map>
#include <atomic>
#include <thread>

struct UvcDevice {
	std::string name;
	std::string user_name;
	std::string serial;
	int vendor_id = 0;
	int product_id = 0;
	bool enabled = false;

	// Internal identifier for native lib
	unsigned int nativeIndex = 0;
	bool isOpened = false;
	bool sync_pending = false;
};

using UvcDevicePtr = std::shared_ptr<UvcDevice>;

class UvcManager {
public:
	UvcManager();
	~UvcManager();

	void RefreshDevices();
	std::vector<UvcDevicePtr> GetDevices();
	void SetDeviceEnabled(const std::string &name, bool enabled);

	void LoadConfig();
	void SaveConfig();

	// PTZ Control
	void SetPanTilt(const std::string &deviceName, int pan, int tilt);
	void SetZoom(const std::string &deviceName, int zoom);
	void BroadcastCapabilities(const std::string &deviceName);
	void SyncAck(const std::string &deviceName);

	// Polling control
	void SetPollingRate(int fps);

	bool IsLoggingEnabled() const { return loggingEnabled; }
	void SetLoggingEnabled(bool enabled) { loggingEnabled = enabled; }

	bool ShouldStartWithObs() const { return startWithObs; }
	void SetStartWithObs(bool enabled) { startWithObs = enabled; }

	std::function<void(const std::string &)> logCallback;
	std::function<void(obs_data_t *packet)> messageCallback;

private:
	void OpenDevice(UvcDevicePtr device);
	void CloseDevice(UvcDevicePtr device);
	void PollingLoop();

	std::vector<UvcDevicePtr> devices;
	std::recursive_mutex devicesMutex;

	std::thread pollingThread;
	std::atomic<bool> stopPolling{false};
	std::atomic<int> pollingIntervalMs{100}; // Default 10 FPS

	bool loggingEnabled = true;
	bool logCollapsed = false;
	bool startWithObs = false;
};

UvcManager &GetUvcManager();
