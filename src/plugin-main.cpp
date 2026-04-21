#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QAction>
#include <QPointer>
#include <QTimer>

#include "uvc-manager.hpp"
#include "uvc-settings-dialog.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("uvc-server", "en-US")

static QPointer<UvcSettingsDialog> settingsDialog;

// Bridge UVC Status to WebSocket Bridge
void HandleStatusToBridge(const std::string &deviceName, const std::string &jsonPayload)
{
	obs_data_t *packet = obs_data_create();
	obs_data_set_string(packet, "a", "uvc_status");
	obs_data_set_string(packet, "device", deviceName.c_str());
	
	obs_data_t *statusData = obs_data_create_from_json(jsonPayload.c_str());
	obs_data_set_obj(packet, "status", statusData);
	obs_data_release(statusData);

	signal_handler_t *sh = obs_get_signal_handler();
	calldata_t cd = {0};
	calldata_set_ptr(&cd, "packet", packet);
	signal_handler_signal(sh, "media_warp_transmit", &cd);
	calldata_free(&cd);
	obs_data_release(packet);
}

// Handle Inbound from Bridge
static void on_media_warp_receive(void *data, calldata_t *cd)
{
	(void)data;
	const char *json_str = calldata_string(cd, "json_str");
	if (!json_str) return;

	obs_data_t *msg = obs_data_create_from_json(json_str);
	if (!msg) return;

	const char *a = obs_data_get_string(msg, "a");
	if (a) {
		if (strcmp(a, "uvc_set_ptz") == 0) {
			const char *deviceName = obs_data_get_string(msg, "device");
			int pan = (int)obs_data_get_int(msg, "pan");
			int tilt = (int)obs_data_get_int(msg, "tilt");
			if (deviceName) GetUvcManager().SetPanTilt(deviceName, pan, tilt);
		} else if (strcmp(a, "uvc_set_zoom") == 0) {
			const char *deviceName = obs_data_get_string(msg, "device");
			int zoom = (int)obs_data_get_int(msg, "zoom");
			if (deviceName) GetUvcManager().SetZoom(deviceName, zoom);
		} else if (strcmp(a, "uvc_set_polling") == 0) {
			int fps = (int)obs_data_get_int(msg, "fps");
			GetUvcManager().SetPollingRate(fps);
		}
	}

	obs_data_release(msg);
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[UVC Server] Plugin loading...");

	auto &mgr = GetUvcManager();
	mgr.LoadConfig();
	mgr.statusCallback = HandleStatusToBridge;

	// Connect to bridge signals
	signal_handler_t *sh = obs_get_signal_handler();
	signal_handler_add(sh, "void media_warp_transmit(ptr packet)");
	signal_handler_connect(sh, "media_warp_receive", on_media_warp_receive, nullptr);

	obs_frontend_add_event_callback(
		[](enum obs_frontend_event event, void * /*param*/) {
			if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
				QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction("UVC Server Settings");
				QObject::connect(action, &QAction::triggered, []() {
					if (!settingsDialog) {
						settingsDialog = new UvcSettingsDialog(
							(QWidget *)obs_frontend_get_main_window());
						settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
					}
					settingsDialog->show();
					settingsDialog->raise();
					settingsDialog->activateWindow();
				});
			}
		},
		nullptr);

	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[UVC Server] Unloading...");

	if (settingsDialog) {
		delete settingsDialog;
	}

	signal_handler_t *sh = obs_get_signal_handler();
	signal_handler_disconnect(sh, "media_warp_receive", on_media_warp_receive, nullptr);

	auto &mgr = GetUvcManager();
	mgr.SaveConfig();
}
