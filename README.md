# UVC Server Plugin

A high-performance OBS Studio plugin for bidirectional control of UVC (USB Video Class) cameras. Part of the **mediaWarp** suite, this plugin enables granular control over camera hardware parameters like Pan, Tilt, and Zoom (PTZ) directly from the OBS ecosystem and web-based overlays.

## Description

The UVC Server plugin bridges the gap between professional camera hardware and digital production environments. It provides a native interface to communicate with UVC-compliant cameras, bypassing standard driver limitations to offer real-time status monitoring and remote hardware control. By integrating with the mediaWarp signal pipeline, it allows browser-based sources or external applications to command camera movements and receive hardware state updates.

## Features

- **Native macOS Hardware Control**: Direct integration with macOS system frameworks for low-latency communication with UVC devices.
- **Granular PTZ Control**: Support for absolute Pan, Tilt, and Zoom adjustments on compatible hardware.
- **Automatic Capability Discovery**: Queries connected cameras to identify supported controls (e.g., exposure, focus, PTZ limits) and broadcasts them to the ecosystem.
- **Real-time Status Polling**: Continuously monitors hardware state and broadcasts updates to keep overlays and controllers in sync.
- **Bidirectional Signal Pipeline**:
    - **Transmit**: Broadcasts camera capabilities and real-time status (PTZ values) via the `media_warp_transmit` OBS signal.
    - **Receive**: Responds to remote commands (`uvc_set_ptz`, `uvc_set_zoom`) via the `media_warp_receive` signal.
- **Device Management**:
    - Enable/Disable specific cameras for control.
    - Assign custom **Aliases** to cameras for simplified routing.
- **Persistent Configuration**: Saves device pairings, enabled states, and user-defined names between sessions.

## Technical Overview

The plugin is architected for maximum hardware compatibility and reliability:

- **Core Engine**: C++17
- **Native Implementation**: Objective-C components utilize `IOKit` and `Foundation` frameworks for direct macOS UVC interaction.
- **UI Framework**: Qt6, providing a native settings interface within the OBS Tools menu.
- **OBS Integration**: Deep integration with `libobs` and `obs-frontend-api` for signal handling and UI lifecycle management.
- **Communication Protocol**: Utilizes the mediaWarp JSON packet structure, ensuring compatibility with WebSocket bridges like the `local-mongoose-webserver`.

## Integration Examples

### Setting PTZ via Signal
To control a camera from a browser source, send a JSON packet to the `media_warp_receive` signal:
```json
{
  "a": "uvc_set_ptz",
  "device": "MainCamera",
  "pan": 500,
  "tilt": -200
}
```

### Receiving Status Updates
The plugin broadcasts the current state of enabled cameras:
```json
{
  "a": "uvc_status",
  "device": "MainCamera",
  "status": {
    "pan": 500,
    "tilt": -200,
    "zoom": 10
  }
}
```

---

## Disclaimer

**THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND**, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---
*Developed as part of the **mediaWarp** ecosystem.*

**Made with Antigravity**
