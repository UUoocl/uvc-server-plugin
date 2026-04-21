# OSC Server Plugin for OBS Studio

A powerful, high-performance bridge for bidirectional OSC (Open Sound Control) and WebSocket communication within OBS Studio.

## Features

- **Bidirectional OSC Support**: Send and receive OSC messages over UDP.
- **Embedded WebSocket Server**: Built-in high-performance Mongoose server for direct communication with Browser Sources.
- **Dynamic Routing**: Route incoming OSC messages from specific devices to selected Browser Sources by name or output port.
- **Auto-Discovery Handshake**: Automatically broadcasts server details (IP, Port, Connected Devices) to all active browser overlays on startup.
- **Persistent Configuration**: Save server settings, device lists, and UI states (like console collapse status) directly in OBS.
- **Rich Settings UI**: Modern Qt-based interface with real-time status indicators and a live log console.

## Tech Stack

- **Core**: C++17
- **Networking**: [Mongoose](https://github.com/cesanta/mongoose) (WebSockets/HTTP)
- **OSC Protocol**: [tinyosc](https://github.com/v923z/tinyosc) (UDP)
- **UI Framework**: Qt 6 (OBS standard)
- **Build System**: CMake

## Installation (Local Development)

To build and deploy the plugin locally on macOS:

1. Clone the repository.
2. Run the deployment script:
   ```bash
   ./deploy_macos.sh
   ```
3. Restart OBS Studio.
4. Open the settings via **Tools > OSC Server Settings**.

## MIDI Server Plugin for OBS Studio

This plugin provides a WebSocket-based bridge between MIDI devices and OBS Browser Sources via the Media Warp centralized bridge.

## Features
- List all connected MIDI devices (Input and Output).
- Enable/Disable specific MIDI devices.
- Bidirectional communication: Send MIDI from devices to overlays, and from overlays to devices.
- Automatic WebSocket discovery.
- "Refresh" button to poll for new MIDI devices without restarting OBS.

## WebSocket Protocol
The plugin transmits and receives MIDI messages via the `media_warp_transmit` and `media_warp_receive` signals.

### MIDI to Browser
MIDI messages are sent to the bridge as JSON packets:
```json
{
  "a": "midi_message",
  "device": "Your MIDI Device Name",
  "data": [{"value": 144}, {"value": 60}, {"value": 127}]
}
```

### Browser to MIDI
Send a JSON message to the bridge to be routed to MIDI devices:
```json
{
  "a": "midi_message",
  "device": "Your MIDI Device Name",
  "data": [{"value": 144}, {"value": 60}, {"value": 100}]
}
```
If `device` is empty, it broadcasts to all enabled output devices.

## Settings
Access settings via **Tools -> MIDI Server Settings**.
- **MIDI Devices**: List of detected devices. 
  - **Label**: (Optional) A user-defined name for the device. Overlays can target this name instead of the system name.
  - **Enable**: Must be checked for the device to be active.
- **Activity Log**: Real-time monitor for all MIDI traffic.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
