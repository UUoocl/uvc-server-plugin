#include "uvc-manager.hpp"
#include <obs-module.h>
#include <util/platform.h>
#include <sstream>
#include <iostream>
#include <chrono>

UvcManager::UvcManager()
{
	uvc_error_t res = uvc_init(&ctx, NULL);
	if (res < 0) {
		blog(LOG_ERROR, "[UVC Server] Failed to initialize libuvc: %s", uvc_strerror(res));
	}
	
	pollingThread = std::thread(&UvcManager::PollingLoop, this);
}

UvcManager::~UvcManager()
{
	stopPolling = true;
	if (pollingThread.joinable())
		pollingThread.join();

	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	for (auto &dev : devices) {
		CloseDevice(dev);
		if (dev->dev) uvc_unref_device(dev->dev);
	}
	
	if (ctx) uvc_exit(ctx);
}

void UvcManager::RefreshDevices()
{
	if (!ctx) return;

	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	uvc_device_t **list;
	uvc_error_t res = uvc_get_device_list(ctx, &list);
	
	if (res < 0) {
		blog(LOG_ERROR, "[UVC Server] Failed to get device list: %s", uvc_strerror(res));
		return;
	}

	std::vector<UvcDevicePtr> newDevices;
	int i = 0;
	while (list[i] != NULL) {
		uvc_device_t *dev = list[i];
		uvc_device_descriptor_t *desc;
		
		if (uvc_get_device_descriptor(dev, &desc) == UVC_SUCCESS) {
			std::string name = desc->product ? desc->product : "Unknown UVC Camera";
			std::string serial = desc->serial ? desc->serial : "";
			int vid = desc->idVendor;
			int pid = desc->idProduct;
			
			// Find existing
			UvcDevicePtr existing = nullptr;
			for (const auto &d : devices) {
				if (d->vendor_id == vid && d->product_id == pid && d->serial == serial) {
					existing = d;
					break;
				}
			}
			
			if (!existing) {
				existing = std::make_shared<UvcDevice>();
				existing->name = name;
				existing->serial = serial;
				existing->vendor_id = vid;
				existing->product_id = pid;
			}
			
			// Always update the uvc_device_t handle as it might have changed on replug
			if (existing->dev) uvc_unref_device(existing->dev);
			existing->dev = dev;
			uvc_ref_device(dev);
			
			newDevices.push_back(existing);
			uvc_free_device_descriptor(desc);
		}
		i++;
	}
	
	uvc_free_device_list(list, 1);
	devices = newDevices;
	
	// Open enabled devices
	for (auto &dev : devices) {
		if (dev->enabled) OpenDevice(dev);
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
			dev->enabled = enabled;
			if (enabled) OpenDevice(dev);
			else CloseDevice(dev);
		}
	}
}

void UvcManager::OpenDevice(UvcDevicePtr device)
{
	if (!device->dev || device->devh) return;
	
	uvc_error_t res = uvc_open(device->dev, &device->devh);
	if (res < 0) {
		blog(LOG_ERROR, "[UVC Server] Failed to open device %s: %s", device->name.c_str(), uvc_strerror(res));
	} else {
		blog(LOG_INFO, "[UVC Server] Opened device: %s", device->name.c_str());
	}
}

void UvcManager::CloseDevice(UvcDevicePtr device)
{
	if (device->devh) {
		uvc_close(device->devh);
		device->devh = nullptr;
		blog(LOG_INFO, "[UVC Server] Closed device: %s", device->name.c_str());
	}
}

void UvcManager::SetPanTilt(const std::string &deviceName, int pan, int tilt)
{
	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	for (auto &dev : devices) {
		if ((dev->name == deviceName || dev->user_name == deviceName) && dev->devh) {
			uvc_set_pantilt_abs(dev->devh, pan, tilt);
			if (logCallback) {
				std::stringstream ss;
				ss << "[UVC] Set PanTilt for " << dev->name << ": " << pan << ", " << tilt;
				logCallback(ss.str());
			}
			return;
		}
	}
}

void UvcManager::SetZoom(const std::string &deviceName, int zoom)
{
	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	for (auto &dev : devices) {
		if ((dev->name == deviceName || dev->user_name == deviceName) && dev->devh) {
			uvc_set_zoom_abs(dev->devh, zoom);
			if (logCallback) {
				std::stringstream ss;
				ss << "[UVC] Set Zoom for " << dev->name << ": " << zoom;
				logCallback(ss.str());
			}
			return;
		}
	}
}

void UvcManager::SetPollingRate(int fps)
{
	if (fps <= 0) pollingIntervalMs = 0; // Disable
	else pollingIntervalMs = 1000 / fps;
}

void UvcManager::PollingLoop()
{
	while (!stopPolling) {
		int interval = pollingIntervalMs;
		if (interval > 0) {
			std::lock_guard<std::recursive_mutex> lock(devicesMutex);
			for (auto &dev : devices) {
				if (dev->enabled && dev->devh) {
					int32_t pan, tilt;
					uint16_t zoom;
					
					// These might fail if the camera doesn't support reading them back
					auto res_pt = uvc_get_pantilt_abs(dev->devh, &pan, &tilt, UVC_GET_CUR);
					auto res_z = uvc_get_zoom_abs(dev->devh, &zoom, UVC_GET_CUR);
					
					if (statusCallback) {
						std::stringstream ss;
						ss << "{\"pan\":" << pan << ",\"tilt\":" << tilt << ",\"zoom\":" << zoom << "}";
						statusCallback(dev->name, ss.str());
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(interval));
		} else {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
}

void UvcManager::LoadConfig()
{
	char *configPath = obs_module_config_path("uvc-server-settings.json");
	if (!configPath) return;

	obs_data_t *data = obs_data_create_from_json_file(configPath);
	bfree(configPath);

	if (data) {
		logCollapsed = obs_data_get_bool(data, "log_collapsed");
		
		std::lock_guard<std::recursive_mutex> lock(devicesMutex);
		obs_data_array_t *devArray = obs_data_get_array(data, "devices");
		if (devArray) {
			size_t count = obs_data_array_count(devArray);
			for (size_t i = 0; i < count; i++) {
				obs_data_t *devData = obs_data_array_item(devArray, i);
				std::string name = obs_data_get_string(devData, "name");
				std::string user_name = obs_data_get_string(devData, "user_name");
				std::string serial = obs_data_get_string(devData, "serial");
				bool enabled = obs_data_get_bool(devData, "enabled");
				
				auto dev = std::make_shared<UvcDevice>();
				dev->name = name;
				dev->user_name = user_name;
				dev->serial = serial;
				dev->enabled = enabled;
				devices.push_back(dev);
				
				obs_data_release(devData);
			}
			obs_data_array_release(devArray);
		}
		obs_data_release(data);
	}
	
	RefreshDevices();
}

void UvcManager::SaveConfig()
{
	char *configPath = obs_module_config_path("uvc-server-settings.json");
	if (!configPath) return;

	obs_data_t *data = obs_data_create();
	obs_data_set_bool(data, "log_collapsed", logCollapsed);

	obs_data_array_t *devArray = obs_data_array_create();
	std::lock_guard<std::recursive_mutex> lock(devicesMutex);
	for (const auto &dev : devices) {
		obs_data_t *devData = obs_data_create();
		obs_data_set_string(devData, "name", dev->name.c_str());
		obs_data_set_string(devData, "user_name", dev->user_name.c_str());
		obs_data_set_string(devData, "serial", dev->serial.c_str());
		obs_data_set_bool(devData, "enabled", dev->enabled);
		obs_data_array_push_back(devArray, devData);
		obs_data_release(devData);
	}
	obs_data_set_array(data, "devices", devArray);
	obs_data_array_release(devArray);

	obs_data_save_json(data, configPath);
	obs_data_release(data);
	bfree(configPath);
}

UvcManager &GetUvcManager()
{
	static UvcManager instance;
	return instance;
}
