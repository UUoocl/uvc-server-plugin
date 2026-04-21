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
#include "libuvc/libuvc.h"

struct UvcDevice {
	std::string name;
	std::string user_name;
	std::string serial;
	int vendor_id = 0;
	int product_id = 0;
	bool enabled = false;
	
	// Internal handles
	uvc_device_t *dev = nullptr;
	uvc_device_handle_t *devh = nullptr;
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
	
	// Polling control
	void SetPollingRate(int fps);
	
	bool IsLoggingEnabled() const { return loggingEnabled; }
	void SetLoggingEnabled(bool enabled) { loggingEnabled = enabled; }
	
	std::function<void(const std::string &)> logCallback;
	std::function<void(const std::string &deviceName, const std::string &jsonPayload)> statusCallback;

private:
	void OpenDevice(UvcDevicePtr device);
	void CloseDevice(UvcDevicePtr device);
	void PollingLoop();

	uvc_context_t *ctx = nullptr;
	std::vector<UvcDevicePtr> devices;
	std::recursive_mutex devicesMutex;
	
	std::thread pollingThread;
	std::atomic<bool> stopPolling{false};
	std::atomic<int> pollingIntervalMs{100}; // Default 10 FPS
	
	bool loggingEnabled = true;
	bool logCollapsed = false;
};

UvcManager &GetUvcManager();
