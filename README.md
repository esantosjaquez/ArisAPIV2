# Sony CrSDK REST API Server

REST API server to control Sony cameras (Alpha/Cinema Line) and CNC machines with GRBL via HTTP from any client on the local network.

## Overview

This server exposes Sony Camera Remote SDK v2.00.00 functions and GRBL/CNC control as REST endpoints, enabling:

- Camera discovery and connection via USB/Ethernet
- Full remote control (capture, recording, focus)
- Read/write properties (ISO, aperture, shutter speed, etc.)
- Live View streaming (MJPEG)
- Content transfer (download photos/videos)
- **CNC/GRBL control** (homing, G0/G1 movements, parameters)
- Real-time events (WebSocket/logs)

## Compatible Cameras

- **Tested**: ILCE-7M4 (Alpha 7 IV)
- **Compatible**: All cameras supported by CrSDK v2.00.00

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              REST API Server                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │ RestServer  │  │ ApiRouter   │  │ MjpegStreamer   │  │ GrblController  │  │
│  │ (httplib)   │  │ (endpoints) │  │ (live view)     │  │ (CNC control)   │  │
│  └──────┬──────┘  └──────┬──────┘  └────────┬────────┘  └────────┬────────┘  │
│         │                │                   │                    │           │
│  ┌──────┴────────────────┴───────────────────┴────────┐  ┌───────┴────────┐  │
│  │                    CameraManager                    │  │   SerialPort   │  │
│  │                    (singleton)                      │  │   (Linux)      │  │
│  └─────────────────────────┬───────────────────────────┘  └───────┬────────┘  │
│                            │                                      │           │
│  ┌─────────────────────────┴───────────────────────────┐          │           │
│  │              CameraDeviceWrapper                     │          │           │
│  │         (IDeviceCallback implementation)             │          │           │
│  └─────────────────────────┬───────────────────────────┘          │           │
└────────────────────────────┼──────────────────────────────────────┼───────────┘
                             │                                      │
                    ┌────────┴────────┐                    ┌────────┴────────┐
                    │  Sony CrSDK     │                    │  Arduino GRBL   │
                    │  libCr_Core.so  │                    │  (Serial 115200)│
                    └────────┬────────┘                    └────────┬────────┘
                             │                                      │
                    ┌────────┴────────┐                    ┌────────┴────────┐
                    │  Camera (USB)   │                    │  CNC Machine    │
                    │  ILCE-7M4       │                    │  (3-axis)       │
                    └─────────────────┘                    └─────────────────┘
```

---

## Project Structure

```
rest_server/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── include/
│   ├── server/
│   │   ├── RestServer.h        # HTTP server wrapper
│   │   ├── WebSocketHandler.h  # WebSocket events (stub)
│   │   └── MjpegStreamer.h     # MJPEG streaming
│   ├── camera/
│   │   ├── CameraManager.h     # SDK lifecycle & camera registry
│   │   └── CameraDeviceWrapper.h # Device wrapper with callbacks
│   ├── api/
│   │   ├── ApiRouter.h         # REST endpoint definitions
│   │   └── JsonHelpers.h       # JSON utilities
│   ├── grbl/
│   │   ├── GrblController.h    # GRBL controller (singleton)
│   │   └── SerialPort.h        # Serial port wrapper (Linux)
│   └── util/                   # (future) utilities
├── src/
│   ├── main.cpp                # Entry point
│   ├── server/
│   │   ├── RestServer.cpp
│   │   ├── WebSocketHandler.cpp
│   │   └── MjpegStreamer.cpp
│   ├── camera/
│   │   ├── CameraManager.cpp
│   │   └── CameraDeviceWrapper.cpp
│   ├── api/
│   │   ├── ApiRouter.cpp
│   │   └── JsonHelpers.cpp
│   └── grbl/
│       ├── GrblController.cpp  # GRBL protocol implementation
│       └── SerialPort.cpp      # Serial I/O (termios)
└── external/
    ├── httplib.h               # cpp-httplib (header-only)
    └── json.hpp                # nlohmann/json (header-only)
```

---

## Dependencies

### System (Ubuntu/Debian)

```bash
sudo apt install -y \
    cmake \
    g++ \
    libboost-all-dev \
    autoconf \
    libtool \
    libudev-dev \
    libxml2-dev
```

### Header-Only Libraries (included)

| Library | Version | Usage |
|---------|---------|-------|
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | 0.15.3 | HTTP server |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | JSON serialization |

---

## Build

```bash
cd /home/resonance/CrSDK_v2.00.00_20251030a_Linux64PC

# Create build directory
mkdir -p build && cd build

# Configure (includes REST server by default)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Compile
cmake --build . -j$(nproc)

# Generated executable
ls -la rest_server/CrSDKRestServer
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_REST_SERVER` | ON | Build the REST server |

---

## Running

```bash
cd build/rest_server

# Run with default options
./CrSDKRestServer

# With custom options
./CrSDKRestServer --host 0.0.0.0 --port 8080 --ws-port 8081

# Help
./CrSDKRestServer --help
```

### Command Line Options

| Option | Default | Description |
|--------|---------|-------------|
| `--host` | 0.0.0.0 | Bind address |
| `--port` | 8080 | HTTP port |
| `--ws-port` | 8081 | WebSocket port |

---

## API Reference

### Standard Response

All responses follow this format:

```json
{
  "success": true,
  "data": { ... },
  "timestamp": "2024-01-08T12:00:00Z"
}
```

On error:

```json
{
  "success": false,
  "error": {
    "code": 33282,
    "message": "Connection timeout",
    "sdkError": "CrError_Connect_TimeOut"
  },
  "timestamp": "2024-01-08T12:00:00Z"
}
```

---

### SDK Lifecycle

#### POST /api/v1/sdk/init
Initialize the Sony SDK.

```bash
curl -X POST http://localhost:8080/api/v1/sdk/init
```

#### POST /api/v1/sdk/release
Release SDK resources.

```bash
curl -X POST http://localhost:8080/api/v1/sdk/release
```

#### GET /api/v1/sdk/version
Get SDK version.

```bash
curl http://localhost:8080/api/v1/sdk/version
```

**Response:**
```json
{
  "success": true,
  "data": {
    "version": 33554432,
    "major": 2,
    "minor": 0,
    "patch": 0
  }
}
```

---

### Cameras

#### GET /api/v1/cameras
List available cameras (scans USB/network).

```bash
curl http://localhost:8080/api/v1/cameras
```

**Response:**
```json
{
  "success": true,
  "data": {
    "cameras": [
      {
        "index": 0,
        "id": "camera-0",
        "model": "ILCE-7M4",
        "connectionType": "USB",
        "sshSupported": false
      }
    ]
  }
}
```

#### GET /api/v1/cameras/connected
List currently connected cameras.

```bash
curl http://localhost:8080/api/v1/cameras/connected
```

#### POST /api/v1/cameras/{index}/connect
Connect to a camera.

**Query params:**
- `mode`: 0=Remote (default), 1=ContentsTransfer
- `reconnect`: true/false (default: true)

```bash
# Remote mode (control)
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=0"

# ContentsTransfer mode (download)
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=1"
```

#### POST /api/v1/cameras/{index}/disconnect
Disconnect a camera.

```bash
curl -X POST http://localhost:8080/api/v1/cameras/0/disconnect
```

---

### Properties

#### GET /api/v1/cameras/{index}/properties
Get all camera properties.

```bash
curl http://localhost:8080/api/v1/cameras/0/properties
```

**Response:**
```json
{
  "success": true,
  "data": {
    "properties": [
      {
        "code": 256,
        "currentValue": 560,
        "possibleValues": [140, 200, 280, 400, 560, 800],
        "writable": true
      }
    ]
  }
}
```

**Common Property Codes:**

| Code | Hex | Property |
|------|-----|----------|
| 256 | 0x0100 | FNumber (aperture) |
| 259 | 0x0103 | ShutterSpeed |
| 260 | 0x0104 | IsoSensitivity |
| 261 | 0x0105 | ExposureProgramMode |
| 264 | 0x0108 | WhiteBalance |
| 265 | 0x0109 | FocusMode |
| 269 | 0x010D | DriveMode |
| 1280 | 0x0500 | BatteryRemain |
| 1296 | 0x0510 | LiveView_Status |

#### PUT /api/v1/cameras/{index}/properties/{code}
Modify a property.

```bash
# Change ISO to 800
curl -X PUT http://localhost:8080/api/v1/cameras/0/properties/260 \
  -H "Content-Type: application/json" \
  -d '{"value": 800}'
```

---

### Commands

#### POST /api/v1/cameras/{index}/command
Send a generic command.

```bash
curl -X POST http://localhost:8080/api/v1/cameras/0/command \
  -H "Content-Type: application/json" \
  -d '{"commandId": 1, "param": 0}'
```

**Common Commands:**

| ID | Hex | Command |
|----|-----|---------|
| 1 | 0x0001 | Release (shutter) |
| 2 | 0x0002 | MovieRecord |
| 513 | 0x0201 | AF_Shutter |
| 514 | 0x0202 | Cancel AF_Shutter |
| 517 | 0x0205 | Half Press |
| 518 | 0x0206 | Release Half Press |

#### POST /api/v1/cameras/{index}/capture
Capture a photo (shutter release).

```bash
curl -X POST http://localhost:8080/api/v1/cameras/0/capture
```

#### POST /api/v1/cameras/{index}/record/start
Start video recording.

```bash
curl -X POST http://localhost:8080/api/v1/cameras/0/record/start
```

#### POST /api/v1/cameras/{index}/record/stop
Stop video recording.

```bash
curl -X POST http://localhost:8080/api/v1/cameras/0/record/stop
```

#### POST /api/v1/cameras/{index}/focus
Focus control.

**Body:**
- `action`: "near" | "far" | "track_start" | "track_stop"
- `step`: 1-7 (optional, for near/far)

```bash
# Focus closer
curl -X POST http://localhost:8080/api/v1/cameras/0/focus \
  -H "Content-Type: application/json" \
  -d '{"action": "near", "step": 3}'
```

---

### Live View

#### GET /api/v1/cameras/{index}/liveview/image
Get a single JPEG frame from live view.

```bash
curl http://localhost:8080/api/v1/cameras/0/liveview/image -o frame.jpg
```

#### GET /api/v1/cameras/{index}/liveview/stream
Continuous MJPEG stream (for browser or VLC).

```bash
# Open in browser
http://localhost:8080/api/v1/cameras/0/liveview/stream

# With VLC
vlc http://localhost:8080/api/v1/cameras/0/liveview/stream

# With ffmpeg
ffmpeg -i http://localhost:8080/api/v1/cameras/0/liveview/stream -c copy output.mkv
```

#### GET /api/v1/cameras/{index}/liveview/info
Live view status information.

```bash
curl http://localhost:8080/api/v1/cameras/0/liveview/info
```

---

### Content Transfer

> **Note:** Requires connection in ContentsTransfer mode (`mode=1`)

#### GET /api/v1/cameras/{index}/contents/folders
List folders on the SD card.

```bash
curl http://localhost:8080/api/v1/cameras/0/contents/folders
```

#### GET /api/v1/cameras/{index}/contents/folders/{folderHandle}
List contents of a folder.

```bash
curl http://localhost:8080/api/v1/cameras/0/contents/folders/1
```

#### GET /api/v1/cameras/{index}/contents/{contentHandle}
File information.

```bash
curl http://localhost:8080/api/v1/cameras/0/contents/12345
```

#### GET /api/v1/cameras/{index}/contents/{contentHandle}/download
Download a file.

```bash
curl http://localhost:8080/api/v1/cameras/0/contents/12345/download -o photo.jpg
```

#### GET /api/v1/cameras/{index}/contents/{contentHandle}/thumbnail
Get file thumbnail.

```bash
curl http://localhost:8080/api/v1/cameras/0/contents/12345/thumbnail -o thumb.jpg
```

---

### Health Check

#### GET /api/v1/health

```bash
curl http://localhost:8080/api/v1/health
```

**Response:**
```json
{
  "success": true,
  "data": {
    "status": "ok",
    "sdkInitialized": true,
    "connectedCameras": 1
  }
}
```

---

## GRBL/CNC Control

Control CNC machines with GRBL firmware (Arduino) via serial port.

### GRBL Connection

#### GET /api/v1/grbl/ports
List available serial ports.

```bash
curl http://localhost:8080/api/v1/grbl/ports
```

**Response:**
```json
{
  "success": true,
  "data": {
    "ports": ["/dev/ttyUSB0", "/dev/ttyACM0"]
  }
}
```

#### POST /api/v1/grbl/connect
Connect to GRBL device.

```bash
# Auto-detect port
curl -X POST http://localhost:8080/api/v1/grbl/connect

# Specific port
curl -X POST http://localhost:8080/api/v1/grbl/connect \
  -H "Content-Type: application/json" \
  -d '{"port": "/dev/ttyUSB0", "baudRate": 115200}'
```

**Response:**
```json
{
  "success": true,
  "data": {
    "connected": true,
    "port": "/dev/ttyUSB0",
    "version": "Grbl 1.1h ['$' for help]"
  }
}
```

#### POST /api/v1/grbl/disconnect
Disconnect from GRBL device.

```bash
curl -X POST http://localhost:8080/api/v1/grbl/disconnect
```

---

### GRBL Status

#### GET /api/v1/grbl/status
Get current status (position, machine state).

```bash
curl http://localhost:8080/api/v1/grbl/status
```

**Response:**
```json
{
  "success": true,
  "data": {
    "state": "Idle",
    "machinePosition": {"x": 0.000, "y": 0.000, "z": 0.000},
    "workPosition": {"x": 0.000, "y": 0.000, "z": 0.000},
    "feed": 0,
    "spindle": 0,
    "override": {"feed": 100, "rapid": 100, "spindle": 100}
  }
}
```

**GRBL States:**

| State | Description |
|-------|-------------|
| `Idle` | Ready, waiting for commands |
| `Run` | Executing movement |
| `Hold` | Paused (feed hold) |
| `Jog` | Jog mode active |
| `Alarm` | Alarm active (requires $X) |
| `Door` | Safety door open |
| `Check` | G-code check mode |
| `Home` | Executing homing |
| `Sleep` | Sleep mode |

---

### Movement

#### POST /api/v1/grbl/home
Execute homing cycle ($H).

```bash
curl -X POST http://localhost:8080/api/v1/grbl/home
```

#### POST /api/v1/grbl/move
Execute G0 (rapid) or G1 (linear) movement.

```bash
# Rapid movement G0
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -H "Content-Type: application/json" \
  -d '{"type": "G0", "x": 100, "y": 50}'

# Linear movement G1 with feed rate
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -H "Content-Type: application/json" \
  -d '{"type": "G1", "x": 100, "y": 50, "z": -5, "feed": 500}'
```

**Parameters:**

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | "G0" (rapid) or "G1" (linear) |
| `x` | number/null | X position (optional) |
| `y` | number/null | Y position (optional) |
| `z` | number/null | Z position (optional) |
| `feed` | number | Feed rate in mm/min (G1 only) |

#### POST /api/v1/grbl/jog
Incremental jog on an axis.

```bash
curl -X POST http://localhost:8080/api/v1/grbl/jog \
  -H "Content-Type: application/json" \
  -d '{"axis": "X", "distance": 10, "feed": 1000}'
```

---

### Control

#### POST /api/v1/grbl/stop
Feed hold - pause current movement (!).

```bash
curl -X POST http://localhost:8080/api/v1/grbl/stop
```

#### POST /api/v1/grbl/resume
Cycle start - resume after hold (~).

```bash
curl -X POST http://localhost:8080/api/v1/grbl/resume
```

#### POST /api/v1/grbl/reset
Soft reset - reset GRBL (0x18).

```bash
curl -X POST http://localhost:8080/api/v1/grbl/reset
```

#### POST /api/v1/grbl/unlock
Unlock after alarm ($X).

```bash
curl -X POST http://localhost:8080/api/v1/grbl/unlock
```

---

### GRBL Parameters

#### GET /api/v1/grbl/settings
List all GRBL parameters ($$).

```bash
curl http://localhost:8080/api/v1/grbl/settings
```

**Response:**
```json
{
  "success": true,
  "data": {
    "settings": [
      {"id": 0, "value": 10, "description": "Step pulse time (microseconds)"},
      {"id": 1, "value": 25, "description": "Step idle delay (milliseconds)"},
      {"id": 100, "value": 250.0, "description": "X steps/mm"},
      {"id": 101, "value": 250.0, "description": "Y steps/mm"},
      {"id": 102, "value": 250.0, "description": "Z steps/mm"},
      {"id": 110, "value": 5000.0, "description": "X max rate (mm/min)"},
      {"id": 111, "value": 5000.0, "description": "Y max rate (mm/min)"},
      {"id": 112, "value": 500.0, "description": "Z max rate (mm/min)"}
    ]
  }
}
```

**Common GRBL Parameters:**

| ID | Description |
|----|-------------|
| $0 | Step pulse time (us) |
| $1 | Step idle delay (ms) |
| $2 | Step port invert mask |
| $3 | Direction port invert mask |
| $4 | Step enable invert |
| $5 | Limit pins invert |
| $6 | Probe pin invert |
| $10 | Status report options |
| $100-102 | Steps/mm (X, Y, Z) |
| $110-112 | Max rate mm/min (X, Y, Z) |
| $120-122 | Acceleration mm/s² (X, Y, Z) |
| $130-132 | Max travel mm (X, Y, Z) |

#### PUT /api/v1/grbl/settings/{id}
Modify a GRBL parameter.

```bash
# Change X axis steps/mm
curl -X PUT http://localhost:8080/api/v1/grbl/settings/100 \
  -H "Content-Type: application/json" \
  -d '{"value": 300.0}'
```

---

### Raw Command

#### POST /api/v1/grbl/command
Send arbitrary G-code command.

```bash
curl -X POST http://localhost:8080/api/v1/grbl/command \
  -H "Content-Type: application/json" \
  -d '{"command": "G90 G21"}'
```

**Response:**
```json
{
  "success": true,
  "data": {
    "command": "G90 G21",
    "response": "ok"
  }
}
```

---

### Typical CNC Workflow

```bash
# 1. Connect
curl -X POST http://localhost:8080/api/v1/grbl/connect

# 2. Check status
curl http://localhost:8080/api/v1/grbl/status

# 3. Home (if limit switches present)
curl -X POST http://localhost:8080/api/v1/grbl/home

# 4. Set absolute mode and millimeters
curl -X POST http://localhost:8080/api/v1/grbl/command \
  -d '{"command": "G90 G21"}'

# 5. Move to starting position
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G0", "x": 0, "y": 0, "z": 5}'

# 6. Lower tool
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G1", "z": -2, "feed": 100}'

# 7. Cut a square
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G1", "x": 50, "feed": 500}'
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G1", "y": 50, "feed": 500}'
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G1", "x": 0, "feed": 500}'
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G1", "y": 0, "feed": 500}'

# 8. Raise tool and return to origin
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G0", "z": 5}'
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G0", "x": 0, "y": 0}'
```

---

## Connection Modes

The SDK supports two mutually exclusive operation modes:

### Remote Mode (mode=0)

For camera remote control:
- Photo/video capture
- Property control
- Live view streaming
- Focus commands

```bash
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=0"
```

### ContentsTransfer Mode (mode=1)

For file transfer:
- List folders and files
- Download photos/videos
- Get thumbnails

```bash
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=1"
```

### Workflow: Capture and Download

```bash
# 1. Connect in Remote mode
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=0"

# 2. Capture photo
curl -X POST http://localhost:8080/api/v1/cameras/0/capture

# 3. Disconnect
curl -X POST http://localhost:8080/api/v1/cameras/0/disconnect

# 4. Reconnect in ContentsTransfer mode
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=1"

# 5. List folders
curl http://localhost:8080/api/v1/cameras/0/contents/folders

# 6. List contents (use handle from step 5)
curl http://localhost:8080/api/v1/cameras/0/contents/folders/1

# 7. Download (use handle from step 6)
curl http://localhost:8080/api/v1/cameras/0/contents/12345/download -o photo.jpg
```

---

## WebSocket Events

> **Note:** Currently implemented as stub (logs to console).
> Real WebSocket requires library compatible with Boost 1.83+.

### Available Events

| Event | Description |
|-------|-------------|
| `connected` | Camera connected |
| `disconnected` | Camera disconnected |
| `property_changed` | Property modified |
| `lv_property_changed` | Live view property changed |
| `capture_complete` | Capture completed |
| `error` | SDK error |
| `warning` | Warning |

### Event Format

```json
{
  "event": "property_changed",
  "cameraIndex": 0,
  "timestamp": "2024-01-08T12:00:00Z",
  "data": {
    "codes": [256, 260, 264]
  }
}
```

---

## HTTP Error Codes

| Code | Meaning |
|------|---------|
| 200 | OK |
| 400 | Bad Request (invalid parameters) |
| 403 | Forbidden (connection rejected) |
| 404 | Not Found (camera not found) |
| 409 | Conflict (camera busy) |
| 500 | Internal Server Error |
| 503 | Service Unavailable (SDK unavailable) |
| 504 | Gateway Timeout (connection timeout) |

---

## Integration Examples

### Python

```python
import requests

BASE_URL = "http://localhost:8080/api/v1"

# List cameras
cameras = requests.get(f"{BASE_URL}/cameras").json()
print(cameras)

# Connect
requests.post(f"{BASE_URL}/cameras/0/connect")

# Capture
requests.post(f"{BASE_URL}/cameras/0/capture")

# Get live view frame
response = requests.get(f"{BASE_URL}/cameras/0/liveview/image")
with open("frame.jpg", "wb") as f:
    f.write(response.content)
```

### JavaScript/Node.js

```javascript
const fetch = require('node-fetch');

const BASE_URL = 'http://localhost:8080/api/v1';

async function capturePhoto() {
  // Connect
  await fetch(`${BASE_URL}/cameras/0/connect`, { method: 'POST' });

  // Capture
  const result = await fetch(`${BASE_URL}/cameras/0/capture`, { method: 'POST' });
  console.log(await result.json());
}

capturePhoto();
```

### cURL Script

```bash
#!/bin/bash
BASE="http://localhost:8080/api/v1"

# Connect and capture 5 photos
curl -X POST "$BASE/cameras/0/connect"

for i in {1..5}; do
  curl -X POST "$BASE/cameras/0/capture"
  sleep 2
done

curl -X POST "$BASE/cameras/0/disconnect"
```

---

## Troubleshooting

### Camera not detected

1. Check USB connection
2. Check permissions: `sudo chmod 666 /dev/bus/usb/*/*`
3. On camera: Menu > Network > PC Remote Function > PC Remote > ON

### Connection error

1. Disconnect and reconnect USB
2. Restart the server
3. Verify no other application is using the camera

### Live view not working

1. Verify camera is in Remote mode
2. Some camera modes disable live view
3. Check `LiveView_Status` property (code 1296)

### GRBL port not detected

1. Verify Arduino is connected: `ls /dev/ttyUSB* /dev/ttyACM*`
2. Check permissions: `sudo chmod 666 /dev/ttyUSB0` or add user to `dialout` group
3. Verify no other application is using the port (Arduino IDE, etc.)

### GRBL in Alarm state

1. Execute unlock: `curl -X POST http://localhost:8080/api/v1/grbl/unlock`
2. If it persists, do soft reset: `curl -X POST http://localhost:8080/api/v1/grbl/reset`
3. Check limit switches and connections

### Movements not responding

1. Check status: `curl http://localhost:8080/api/v1/grbl/status`
2. If in Hold, resume: `curl -X POST http://localhost:8080/api/v1/grbl/resume`
3. Verify feed rate values are reasonable (e.g., 100-5000 mm/min)

---

## Future Development

- [ ] Real WebSocket with compatible library
- [ ] Automatic post-capture download
- [ ] Authentication (API keys)
- [ ] Rate limiting
- [ ] Improved multi-camera simultaneous support
- [ ] OpenAPI/Swagger documentation
- [ ] Docker container
- [x] ~~CNC/GRBL control~~ (implemented)
- [ ] G-code file streaming for CNC
- [ ] Camera-CNC coordination (automated timelapse)

---

## License

This project uses the Sony Camera Remote SDK under Sony's license terms.

---

## Credits

- **Sony Camera Remote SDK** v2.00.00
- **cpp-httplib** - Yuji Hirose
- **nlohmann/json** - Niels Lohmann

---

*Generated for CrSDK REST API Server - Sony ILCE-7M4*
