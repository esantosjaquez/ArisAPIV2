# Sony CrSDK REST API Server

REST API server para controlar cámaras Sony (Alpha/Cinema Line) y CNC con GRBL a través de HTTP desde cualquier cliente en la red local.

## Resumen

Este servidor expone las funciones del Sony Camera Remote SDK v2.00.00 y control CNC/GRBL como endpoints REST, permitiendo:

- Descubrimiento y conexión de cámaras via USB/Ethernet
- Control remoto completo (captura, grabación, enfoque)
- Lectura/escritura de propiedades (ISO, apertura, velocidad, etc.)
- Streaming de Live View (MJPEG)
- Transferencia de contenido (descarga de fotos/videos)
- **Control CNC/GRBL** (homing, movimientos G0/G1, parámetros)
- Eventos en tiempo real (WebSocket/logs)

## Cámaras Compatibles

- **Probado**: ILCE-7M4 (Alpha 7 IV)
- **Compatibles**: Todas las cámaras soportadas por CrSDK v2.00.00

---

## Arquitectura

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

## Estructura del Proyecto

```
rest_server/
├── CMakeLists.txt              # Build configuration
├── README.md                   # Este archivo
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
│   └── util/                   # (futuro) utilidades
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

## Dependencias

### Sistema (Ubuntu/Debian)

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

### Bibliotecas Header-Only (incluidas)

| Biblioteca | Versión | Uso |
|------------|---------|-----|
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | 0.15.3 | HTTP server |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | JSON serialization |

---

## Build

```bash
cd /home/resonance/CrSDK_v2.00.00_20251030a_Linux64PC

# Crear directorio de build
mkdir -p build && cd build

# Configurar (incluye REST server por defecto)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Compilar
cmake --build . -j$(nproc)

# Ejecutable generado
ls -la rest_server/CrSDKRestServer
```

### Opciones de CMake

| Opción | Default | Descripción |
|--------|---------|-------------|
| `BUILD_REST_SERVER` | ON | Compilar el REST server |

---

## Ejecución

```bash
cd build/rest_server

# Ejecutar con opciones por defecto
./CrSDKRestServer

# Con opciones personalizadas
./CrSDKRestServer --host 0.0.0.0 --port 8080 --ws-port 8081

# Ayuda
./CrSDKRestServer --help
```

### Opciones de línea de comandos

| Opción | Default | Descripción |
|--------|---------|-------------|
| `--host` | 0.0.0.0 | Dirección de bind |
| `--port` | 8080 | Puerto HTTP |
| `--ws-port` | 8081 | Puerto WebSocket |

---

## API Reference

### Respuesta estándar

Todas las respuestas siguen este formato:

```json
{
  "success": true,
  "data": { ... },
  "timestamp": "2024-01-08T12:00:00Z"
}
```

En caso de error:

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
Inicializa el SDK de Sony.

```bash
curl -X POST http://localhost:8080/api/v1/sdk/init
```

#### POST /api/v1/sdk/release
Libera recursos del SDK.

```bash
curl -X POST http://localhost:8080/api/v1/sdk/release
```

#### GET /api/v1/sdk/version
Obtiene versión del SDK.

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

### Cámaras

#### GET /api/v1/cameras
Lista cámaras disponibles (escanea USB/red).

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
Lista cámaras actualmente conectadas.

```bash
curl http://localhost:8080/api/v1/cameras/connected
```

#### POST /api/v1/cameras/{index}/connect
Conecta a una cámara.

**Query params:**
- `mode`: 0=Remote (default), 1=ContentsTransfer
- `reconnect`: true/false (default: true)

```bash
# Modo Remote (control)
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=0"

# Modo ContentsTransfer (descarga)
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=1"
```

#### POST /api/v1/cameras/{index}/disconnect
Desconecta una cámara.

```bash
curl -X POST http://localhost:8080/api/v1/cameras/0/disconnect
```

---

### Propiedades

#### GET /api/v1/cameras/{index}/properties
Obtiene todas las propiedades de la cámara.

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

**Códigos de propiedades comunes:**

| Código | Hex | Propiedad |
|--------|-----|-----------|
| 256 | 0x0100 | FNumber (apertura) |
| 259 | 0x0103 | ShutterSpeed |
| 260 | 0x0104 | IsoSensitivity |
| 261 | 0x0105 | ExposureProgramMode |
| 264 | 0x0108 | WhiteBalance |
| 265 | 0x0109 | FocusMode |
| 269 | 0x010D | DriveMode |
| 1280 | 0x0500 | BatteryRemain |
| 1296 | 0x0510 | LiveView_Status |

#### PUT /api/v1/cameras/{index}/properties/{code}
Modifica una propiedad.

```bash
# Cambiar ISO a 800
curl -X PUT http://localhost:8080/api/v1/cameras/0/properties/260 \
  -H "Content-Type: application/json" \
  -d '{"value": 800}'
```

---

### Comandos

#### POST /api/v1/cameras/{index}/command
Envía un comando genérico.

```bash
curl -X POST http://localhost:8080/api/v1/cameras/0/command \
  -H "Content-Type: application/json" \
  -d '{"commandId": 1, "param": 0}'
```

**Comandos comunes:**

| ID | Hex | Comando |
|----|-----|---------|
| 1 | 0x0001 | Release (shutter) |
| 2 | 0x0002 | MovieRecord |
| 513 | 0x0201 | AF_Shutter |
| 514 | 0x0202 | Cancel AF_Shutter |
| 517 | 0x0205 | Half Press |
| 518 | 0x0206 | Release Half Press |

#### POST /api/v1/cameras/{index}/capture
Captura una foto (shutter release).

```bash
curl -X POST http://localhost:8080/api/v1/cameras/0/capture
```

#### POST /api/v1/cameras/{index}/record/start
Inicia grabación de video.

```bash
curl -X POST http://localhost:8080/api/v1/cameras/0/record/start
```

#### POST /api/v1/cameras/{index}/record/stop
Detiene grabación de video.

```bash
curl -X POST http://localhost:8080/api/v1/cameras/0/record/stop
```

#### POST /api/v1/cameras/{index}/focus
Control de enfoque.

**Body:**
- `action`: "near" | "far" | "track_start" | "track_stop"
- `step`: 1-7 (opcional, para near/far)

```bash
# Enfocar más cerca
curl -X POST http://localhost:8080/api/v1/cameras/0/focus \
  -H "Content-Type: application/json" \
  -d '{"action": "near", "step": 3}'
```

---

### Live View

#### GET /api/v1/cameras/{index}/liveview/image
Obtiene un frame JPEG del live view.

```bash
curl http://localhost:8080/api/v1/cameras/0/liveview/image -o frame.jpg
```

#### GET /api/v1/cameras/{index}/liveview/stream
Stream MJPEG continuo (para navegador o VLC).

```bash
# Abrir en navegador
http://localhost:8080/api/v1/cameras/0/liveview/stream

# Con VLC
vlc http://localhost:8080/api/v1/cameras/0/liveview/stream

# Con ffmpeg
ffmpeg -i http://localhost:8080/api/v1/cameras/0/liveview/stream -c copy output.mkv
```

#### GET /api/v1/cameras/{index}/liveview/info
Información del estado del live view.

```bash
curl http://localhost:8080/api/v1/cameras/0/liveview/info
```

---

### Transferencia de Contenido

> **Nota:** Requiere conexión en modo ContentsTransfer (`mode=1`)

#### GET /api/v1/cameras/{index}/contents/folders
Lista carpetas en la tarjeta SD.

```bash
curl http://localhost:8080/api/v1/cameras/0/contents/folders
```

#### GET /api/v1/cameras/{index}/contents/folders/{folderHandle}
Lista contenidos de una carpeta.

```bash
curl http://localhost:8080/api/v1/cameras/0/contents/folders/1
```

#### GET /api/v1/cameras/{index}/contents/{contentHandle}
Información de un archivo.

```bash
curl http://localhost:8080/api/v1/cameras/0/contents/12345
```

#### GET /api/v1/cameras/{index}/contents/{contentHandle}/download
Descarga un archivo.

```bash
curl http://localhost:8080/api/v1/cameras/0/contents/12345/download -o foto.jpg
```

#### GET /api/v1/cameras/{index}/contents/{contentHandle}/thumbnail
Obtiene thumbnail de un archivo.

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

Control de máquinas CNC con firmware GRBL (Arduino) a través de puerto serial.

### Conexión GRBL

#### GET /api/v1/grbl/ports
Lista puertos seriales disponibles.

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
Conecta al dispositivo GRBL.

```bash
# Auto-detectar puerto
curl -X POST http://localhost:8080/api/v1/grbl/connect

# Puerto específico
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
Desconecta del dispositivo GRBL.

```bash
curl -X POST http://localhost:8080/api/v1/grbl/disconnect
```

---

### Estado GRBL

#### GET /api/v1/grbl/status
Obtiene estado actual (posición, estado de la máquina).

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

**Estados GRBL:**

| Estado | Descripción |
|--------|-------------|
| `Idle` | Listo, esperando comandos |
| `Run` | Ejecutando movimiento |
| `Hold` | Pausado (feed hold) |
| `Jog` | Modo jog activo |
| `Alarm` | Alarma activa (requiere $X) |
| `Door` | Puerta de seguridad abierta |
| `Check` | Modo verificación G-code |
| `Home` | Ejecutando homing |
| `Sleep` | Modo sleep |

---

### Movimiento

#### POST /api/v1/grbl/home
Ejecuta ciclo de homing ($H).

```bash
curl -X POST http://localhost:8080/api/v1/grbl/home
```

#### POST /api/v1/grbl/move
Ejecuta movimiento G0 (rápido) o G1 (lineal).

```bash
# Movimiento rápido G0
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -H "Content-Type: application/json" \
  -d '{"type": "G0", "x": 100, "y": 50}'

# Movimiento lineal G1 con feed rate
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -H "Content-Type: application/json" \
  -d '{"type": "G1", "x": 100, "y": 50, "z": -5, "feed": 500}'
```

**Parámetros:**

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `type` | string | "G0" (rapid) o "G1" (linear) |
| `x` | number/null | Posición X (opcional) |
| `y` | number/null | Posición Y (opcional) |
| `z` | number/null | Posición Z (opcional) |
| `feed` | number | Feed rate en mm/min (solo G1) |

#### POST /api/v1/grbl/jog
Jog incremental en un eje.

```bash
curl -X POST http://localhost:8080/api/v1/grbl/jog \
  -H "Content-Type: application/json" \
  -d '{"axis": "X", "distance": 10, "feed": 1000}'
```

---

### Control

#### POST /api/v1/grbl/stop
Feed hold - pausa el movimiento actual (!).

```bash
curl -X POST http://localhost:8080/api/v1/grbl/stop
```

#### POST /api/v1/grbl/resume
Cycle start - reanuda después de hold (~).

```bash
curl -X POST http://localhost:8080/api/v1/grbl/resume
```

#### POST /api/v1/grbl/reset
Soft reset - reinicia GRBL (0x18).

```bash
curl -X POST http://localhost:8080/api/v1/grbl/reset
```

#### POST /api/v1/grbl/unlock
Desbloquea después de alarma ($X).

```bash
curl -X POST http://localhost:8080/api/v1/grbl/unlock
```

---

### Parámetros GRBL

#### GET /api/v1/grbl/settings
Lista todos los parámetros GRBL ($$).

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

**Parámetros GRBL comunes:**

| ID | Descripción |
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
Modifica un parámetro GRBL.

```bash
# Cambiar steps/mm del eje X
curl -X PUT http://localhost:8080/api/v1/grbl/settings/100 \
  -H "Content-Type: application/json" \
  -d '{"value": 300.0}'
```

---

### Comando Raw

#### POST /api/v1/grbl/command
Envía comando G-code arbitrario.

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

### Workflow CNC Típico

```bash
# 1. Conectar
curl -X POST http://localhost:8080/api/v1/grbl/connect

# 2. Verificar estado
curl http://localhost:8080/api/v1/grbl/status

# 3. Hacer homing (si hay limit switches)
curl -X POST http://localhost:8080/api/v1/grbl/home

# 4. Configurar modo absoluto y milímetros
curl -X POST http://localhost:8080/api/v1/grbl/command \
  -d '{"command": "G90 G21"}'

# 5. Mover a posición inicial
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G0", "x": 0, "y": 0, "z": 5}'

# 6. Bajar herramienta
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G1", "z": -2, "feed": 100}'

# 7. Cortar un cuadrado
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G1", "x": 50, "feed": 500}'
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G1", "y": 50, "feed": 500}'
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G1", "x": 0, "feed": 500}'
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G1", "y": 0, "feed": 500}'

# 8. Subir herramienta y volver a origen
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G0", "z": 5}'
curl -X POST http://localhost:8080/api/v1/grbl/move \
  -d '{"type": "G0", "x": 0, "y": 0}'
```

---

## Modos de Conexión

El SDK soporta dos modos de operación exclusivos:

### Remote Mode (mode=0)

Para control remoto de la cámara:
- Captura de fotos/video
- Control de propiedades
- Live view streaming
- Comandos de enfoque

```bash
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=0"
```

### ContentsTransfer Mode (mode=1)

Para transferencia de archivos:
- Listar carpetas y archivos
- Descargar fotos/videos
- Obtener thumbnails

```bash
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=1"
```

### Workflow: Capturar y Descargar

```bash
# 1. Conectar en modo Remote
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=0"

# 2. Capturar foto
curl -X POST http://localhost:8080/api/v1/cameras/0/capture

# 3. Desconectar
curl -X POST http://localhost:8080/api/v1/cameras/0/disconnect

# 4. Reconectar en ContentsTransfer
curl -X POST "http://localhost:8080/api/v1/cameras/0/connect?mode=1"

# 5. Listar carpetas
curl http://localhost:8080/api/v1/cameras/0/contents/folders

# 6. Listar contenido (usar handle de paso 5)
curl http://localhost:8080/api/v1/cameras/0/contents/folders/1

# 7. Descargar (usar handle de paso 6)
curl http://localhost:8080/api/v1/cameras/0/contents/12345/download -o foto.jpg
```

---

## WebSocket Events

> **Nota:** Actualmente implementado como stub (logs a consola).
> WebSocket real requiere biblioteca compatible con Boost 1.83+.

### Eventos disponibles

| Evento | Descripción |
|--------|-------------|
| `connected` | Cámara conectada |
| `disconnected` | Cámara desconectada |
| `property_changed` | Propiedad modificada |
| `lv_property_changed` | Propiedad de live view cambiada |
| `capture_complete` | Captura completada |
| `error` | Error del SDK |
| `warning` | Advertencia |

### Formato de evento

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

## Códigos de Error HTTP

| Código | Significado |
|--------|-------------|
| 200 | OK |
| 400 | Bad Request (parámetros inválidos) |
| 403 | Forbidden (conexión rechazada) |
| 404 | Not Found (cámara no encontrada) |
| 409 | Conflict (cámara ocupada) |
| 500 | Internal Server Error |
| 503 | Service Unavailable (SDK no disponible) |
| 504 | Gateway Timeout (timeout de conexión) |

---

## Ejemplos de Integración

### Python

```python
import requests

BASE_URL = "http://localhost:8080/api/v1"

# Listar cámaras
cameras = requests.get(f"{BASE_URL}/cameras").json()
print(cameras)

# Conectar
requests.post(f"{BASE_URL}/cameras/0/connect")

# Capturar
requests.post(f"{BASE_URL}/cameras/0/capture")

# Obtener frame de live view
response = requests.get(f"{BASE_URL}/cameras/0/liveview/image")
with open("frame.jpg", "wb") as f:
    f.write(response.content)
```

### JavaScript/Node.js

```javascript
const fetch = require('node-fetch');

const BASE_URL = 'http://localhost:8080/api/v1';

async function capturePhoto() {
  // Conectar
  await fetch(`${BASE_URL}/cameras/0/connect`, { method: 'POST' });

  // Capturar
  const result = await fetch(`${BASE_URL}/cameras/0/capture`, { method: 'POST' });
  console.log(await result.json());
}

capturePhoto();
```

### cURL Script

```bash
#!/bin/bash
BASE="http://localhost:8080/api/v1"

# Conectar y capturar 5 fotos
curl -X POST "$BASE/cameras/0/connect"

for i in {1..5}; do
  curl -X POST "$BASE/cameras/0/capture"
  sleep 2
done

curl -X POST "$BASE/cameras/0/disconnect"
```

---

## Troubleshooting

### La cámara no aparece

1. Verificar conexión USB
2. Verificar permisos: `sudo chmod 666 /dev/bus/usb/*/*`
3. En la cámara: Menu > Network > PC Remote Function > PC Remote > ON

### Error de conexión

1. Desconectar y reconectar USB
2. Reiniciar el servidor
3. Verificar que no hay otra aplicación usando la cámara

### Live view no funciona

1. Verificar que la cámara está en modo Remote
2. Algunos modos de la cámara deshabilitan live view
3. Verificar propiedad `LiveView_Status` (código 1296)

### GRBL no detecta el puerto

1. Verificar que el Arduino está conectado: `ls /dev/ttyUSB* /dev/ttyACM*`
2. Verificar permisos: `sudo chmod 666 /dev/ttyUSB0` o agregar usuario al grupo `dialout`
3. Verificar que no hay otra aplicación usando el puerto (Arduino IDE, etc.)

### GRBL en estado Alarm

1. Ejecutar unlock: `curl -X POST http://localhost:8080/api/v1/grbl/unlock`
2. Si persiste, hacer soft reset: `curl -X POST http://localhost:8080/api/v1/grbl/reset`
3. Verificar limit switches y conexiones

### Movimientos no responden

1. Verificar estado: `curl http://localhost:8080/api/v1/grbl/status`
2. Si está en Hold, hacer resume: `curl -X POST http://localhost:8080/api/v1/grbl/resume`
3. Verificar que los valores de feed rate son razonables (ej: 100-5000 mm/min)

---

## Desarrollo Futuro

- [ ] WebSocket real con biblioteca compatible
- [ ] Descarga automática post-captura
- [ ] Autenticación (API keys)
- [ ] Rate limiting
- [ ] Soporte multi-cámara simultáneo mejorado
- [ ] Documentación OpenAPI/Swagger
- [ ] Contenedor Docker
- [x] ~~Control CNC/GRBL~~ (implementado)
- [ ] G-code file streaming para CNC
- [ ] Coordinación cámara-CNC (timelapse automatizado)

---

## Licencia

Este proyecto utiliza el Sony Camera Remote SDK bajo los términos de licencia de Sony.

---

## Créditos

- **Sony Camera Remote SDK** v2.00.00
- **cpp-httplib** - Yuji Hirose
- **nlohmann/json** - Niels Lohmann

---

*Generado para CrSDK REST API Server - Sony ILCE-7M4*
