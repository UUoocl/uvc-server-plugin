#include "uvc-manager.hpp"
#include <obs-module.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <chrono>
#include <sstream>

extern "C" {
int uvclib_refresh_devices();
const char *uvclib_get_devices_json();
int uvclib_select_device(unsigned int index);
const char *uvclib_get_controls_json();
const char *uvclib_get_value(const char *control_name);
const char *uvclib_set_value(const char *control_name, const char *value_str);
}

UvcManager::UvcManager()
{
	LoadConfig();
	pollingThread = std::thread(&UvcManager::PollingLoop, this);
}

UvcManager::~UvcManager()
{
	stopPolling = true;
	if (pollingThread.joinable()) {
		pollingThread.join();
	}

	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	for (auto &device : devices) {
		CloseDevice(device);
	}
}

void UvcManager::RefreshDevices()
{
	std::lock_guard<std::recursive_mutex> lock(devicesMutex);

	int rawCount = uvclib_refresh_devices();
	const char *jsonStr = uvclib_get_devices_json();

	if (loggingEnabled) {
		blog(LOG_INFO, "[UVC Server] RefreshDevices: found %d raw UVC devices", rawCount);
	}

	if (!jsonStr)
		return;

	std::string wrapped = "{\"root\":" + std::string(jsonStr) + "}";
	obs_data_t *data = obs_data_create_from_json(wrapped.c_str());
	if (!data)
		return;

	obs_data_array_t *items = obs_data_get_array(data, "root");
	if (!items) {
		obs_data_release(data);
		return;
	}

	size_t count = obs_data_array_count(items);

	// Mark all existing devices as not found for this refresh
	for (auto &dev : devices) {
		dev->nativeIndex = 9999;
	}

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(items, i);

		std::string name = obs_data_get_string(item, "name");
		int vid = (int)obs_data_get_int(item, "vendorId");
		int pid = (int)obs_data_get_int(item, "productId");
		unsigned int index = (unsigned int)obs_data_get_int(item, "index");

		bool found = false;
		for (auto &dev : devices) {
			if (dev->vendor_id == vid && dev->product_id == pid && dev->name == name) {
				dev->nativeIndex = index;
				found = true;
				break;
			}
		}

		if (!found) {
			auto newDev = std::make_shared<UvcDevice>();
			newDev->name = name;
			newDev->vendor_id = vid;
			newDev->product_id = pid;
			newDev->nativeIndex = index;
			newDev->enabled = false;
			devices.push_back(newDev);

			if (loggingEnabled) {
				blog(LOG_INFO, "[UVC Server] Found new device: %s (VID:%04x PID:%04x)", name.c_str(),
				     vid, pid);
			}
		}

		obs_data_release(item);
	}

	obs_data_array_release(items);
	obs_data_release(data);

	// Re-sync with config (in case new devices were found)
	LoadConfig();

	// Auto-open enabled devices that have a valid nativeIndex
	for (auto &dev : devices) {
		if (dev->enabled && dev->nativeIndex != 9999 && !dev->isOpened) {
			OpenDevice(dev);
		}
	}
}

std::vector<UvcDevicePtr> UvcManager::GetDevices()
{
	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	return devices;
}

void UvcManager::SetDeviceEnabled(const std::string &name, bool enabled)
{
	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	for (auto &dev : devices) {
		if (dev->name == name) {
			if (dev->enabled != enabled) {
				dev->enabled = enabled;
				if (loggingEnabled) {
					blog(LOG_INFO, "[UVC Server] SetDeviceEnabled: '%s' -> %s", name.c_str(),
					     enabled ? "true" : "false");
				}
				if (enabled) {
					OpenDevice(dev);
				} else {
					CloseDevice(dev);
				}
				SaveConfig();
			} else if (enabled && !dev->isOpened) {
				// If it's already enabled but not opened (e.g. just plugged in), try to open it
				OpenDevice(dev);
			}
			break;
		}
	}
}

void UvcManager::OpenDevice(UvcDevicePtr device)
{
	if (device->nativeIndex == 9999) {
		if (loggingEnabled)
			blog(LOG_WARNING, "[UVC Server] Cannot open '%s': Invalid native index", device->name.c_str());
		return;
	}

	if (loggingEnabled) {
		blog(LOG_INFO, "[UVC Server] Attempting to open native control for '%s' (index %u)",
		     device->name.c_str(), device->nativeIndex);
	}

	int res = uvclib_select_device(device->nativeIndex);
	if (res == 0) {
		device->isOpened = true;
		if (loggingEnabled) {
			blog(LOG_INFO, "[UVC Server] Native control handle opened successfully for '%s'",
			     device->name.c_str());
		}
		BroadcastCapabilities(device->name);
	} else {
		device->isOpened = false;
		if (loggingEnabled) {
			blog(LOG_ERROR, "[UVC Server] Failed to select device '%s' for native control (result: %d)",
			     device->name.c_str(), res);
		}
	}
}

void UvcManager::CloseDevice(UvcDevicePtr device)
{
	device->isOpened = false;
	// Native lib doesn't have an explicit close for selected device, just deselect or select another
}

void UvcManager::SetPanTilt(const std::string &deviceName, int pan, int tilt)
{
	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	for (auto &dev : devices) {
		if (dev->name == deviceName && dev->isOpened) {
			uvclib_select_device(dev->nativeIndex);

			std::stringstream ss;
			ss << "{pan=" << pan << ", tilt=" << tilt << "}";
			const char *res = uvclib_set_value("pan-tilt-abs", ss.str().c_str());

			if (loggingEnabled && res) {
				blog(LOG_INFO, "[UVC Server] SetPanTilt result: %s", res);
			}
			break;
		}
	}
}

void UvcManager::SyncAck(const std::string &deviceName)
{
	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	for (auto &dev : devices) {
		if (dev->name == deviceName) {
			dev->sync_pending = false;
			if (loggingEnabled) {
				blog(LOG_INFO, "[UVC Server] Sync acknowledged for '%s'", deviceName.c_str());
			}
			break;
		}
	}
}

void UvcManager::BroadcastCapabilities(const std::string &deviceName)
{
	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	for (auto &dev : devices) {
		if (dev->name == deviceName && dev->isOpened) {
			uvclib_select_device(dev->nativeIndex);
			const char *controlsJson = uvclib_get_controls_json();

			if (loggingEnabled && controlsJson) {
				blog(LOG_INFO, "[UVC Server] Discovered capabilities for '%s': %s", deviceName.c_str(),
				     controlsJson);
			}

			if (controlsJson) {
				if (messageCallback) {
					// Wrap in uvc_capabilities packet
					obs_data_t *packet = obs_data_create();
					obs_data_set_string(packet, "a", "uvc_capabilities");
					obs_data_set_string(packet, "device", deviceName.c_str());

					std::string wrappedJson = "{\"c\":" + std::string(controlsJson) + "}";
					obs_data_t *wrappedData = obs_data_create_from_json(wrappedJson.c_str());
					if (wrappedData) {
						obs_data_array_t *controlsArray = obs_data_get_array(wrappedData, "c");
						obs_data_set_array(packet, "controls", controlsArray);
						obs_data_array_release(controlsArray);
						obs_data_release(wrappedData);
					}

					messageCallback(packet);
					obs_data_release(packet);
				}
			}
			break;
		}
	}
}

void UvcManager::SetZoom(const std::string &deviceName, int zoom)
{
	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	for (auto &dev : devices) {
		if (dev->name == deviceName && dev->isOpened) {
			uvclib_select_device(dev->nativeIndex);

			std::string val = std::to_string(zoom);
			const char *res = uvclib_set_value("zoom-abs", val.c_str());

			if (loggingEnabled && res) {
				blog(LOG_INFO, "[UVC Server] SetZoom result: %s", res);
			}
			break;
		}
	}
}

void UvcManager::SetPollingRate(int fps)
{
	if (fps <= 0)
		fps = 1;
	if (fps > 60)
		fps = 60;
	pollingIntervalMs = 1000 / fps;
	if (loggingEnabled) {
		blog(LOG_INFO, "[UVC Server] Polling rate set to %d FPS (%d ms)", fps, pollingIntervalMs.load());
	}
}

void UvcManager::PollingLoop()
{
	while (!stopPolling) {
		auto start = std::chrono::steady_clock::now();

		{
			std::lock_guard<std::recursive_mutex> lock(devicesMutex);
			bool anyPolled = false;

			for (auto &dev : devices) {
				if (dev->enabled && dev->isOpened && dev->nativeIndex != 9999) {
					uvclib_select_device(dev->nativeIndex);

					const char *ptRaw = uvclib_get_value("pan-tilt-abs");
					std::string ptJson = ptRaw ? ptRaw : "";

					const char *zRaw = uvclib_get_value("zoom-abs");
					std::string zJson = zRaw ? zRaw : "";

					if (messageCallback && (!ptJson.empty() || !zJson.empty())) {
						obs_data_t *payload = obs_data_create();
						obs_data_set_string(payload, "a", "uvc_status");
						obs_data_set_string(payload, "device", dev->name.c_str());

						obs_data_t *statusData = obs_data_create();

						if (!ptJson.empty()) {
							obs_data_t *ptData = obs_data_create_from_json(ptJson.c_str());
							obs_data_t *ptVal = obs_data_get_obj(ptData, "value");
							if (ptVal) {
								obs_data_set_int(statusData, "pan",
										 obs_data_get_int(ptVal, "pan"));
								obs_data_set_int(statusData, "tilt",
										 obs_data_get_int(ptVal, "tilt"));
								obs_data_release(ptVal);
							}
							obs_data_release(ptData);
						}

						if (!zJson.empty()) {
							obs_data_t *zData = obs_data_create_from_json(zJson.c_str());
							obs_data_t *zVal = obs_data_get_obj(zData, "value");
							if (zVal) {
								obs_data_set_int(statusData, "zoom",
										 obs_data_get_int(zVal, "zoom"));
								obs_data_release(zVal);
							}
							obs_data_release(zData);
						}

						obs_data_set_obj(payload, "status", statusData);
						messageCallback(payload);

						obs_data_release(statusData);
						obs_data_release(payload);
					}
					anyPolled = true;
				}
			}

			if (!anyPolled && loggingEnabled && !logCollapsed) {
				// blog(LOG_DEBUG, "[UVC Server] Polling: No active devices");
			}
		}

		auto end = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		int sleepMs = pollingIntervalMs - elapsed;
		if (sleepMs > 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
		}
	}
}

void UvcManager::LoadConfig()
{
	char *configPath = obs_module_config_path("uvc-devices.json");
	if (!configPath)
		return;

	obs_data_t *data = obs_data_create_from_json_file(configPath);
	bfree(configPath);

	if (!data)
		return;

	obs_data_array_t *items = obs_data_get_array(data, "devices");
	if (items) {
		size_t count = obs_data_array_count(items);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item = obs_data_array_item(items, i);
			const char *name = obs_data_get_string(item, "name");
			bool enabled = obs_data_get_bool(item, "enabled");
			int vid = (int)obs_data_get_int(item, "vendor_id");
			int pid = (int)obs_data_get_int(item, "product_id");

			for (auto &dev : devices) {
				// Match by VID/PID first, then name
				if ((dev->vendor_id == vid && dev->product_id == pid) ||
				    (vid == 0 && dev->name == name)) {
					dev->enabled = enabled;
					const char *alias = obs_data_get_string(item, "alias");
					if (alias && *alias)
						dev->user_name = alias;

					if (dev->enabled) {
						OpenDevice(dev);
					}
					break;
				}
			}
			obs_data_release(item);
		}
		obs_data_array_release(items);
	}

	startWithObs = obs_data_get_bool(data, "start_with_obs");
	obs_data_release(data);
}

void UvcManager::SaveConfig()
{
	char *configPath = obs_module_config_path("uvc-devices.json");
	if (!configPath)
		return;

	obs_data_t *data = obs_data_create();
	obs_data_set_bool(data, "start_with_obs", startWithObs);

	obs_data_array_t *items = obs_data_array_create();

	{
		std::lock_guard<std::recursive_mutex> lock(devicesMutex);
		for (const auto &dev : devices) {
			obs_data_t *item = obs_data_create();
			obs_data_set_string(item, "name", dev->name.c_str());
			obs_data_set_string(item, "alias", dev->user_name.c_str());
			obs_data_set_bool(item, "enabled", dev->enabled);
			obs_data_set_int(item, "vendor_id", dev->vendor_id);
			obs_data_set_int(item, "product_id", dev->product_id);
			obs_data_array_push_back(items, item);
			obs_data_release(item);
		}
	}
	obs_data_set_array(data, "devices", items);
	obs_data_array_release(items);

	obs_data_save_json(data, configPath);
	bfree(configPath);
	obs_data_release(data);
}

static std::unique_ptr<UvcManager> uvcManager;

UvcManager &GetUvcManager()
{
	if (!uvcManager) {
		uvcManager = std::make_unique<UvcManager>();
	}
	return *uvcManager;
}
