# Python gRPC Client for GeometryServer

## Overview
Professional Python client with PythonOCC-core for AIS-based 3D visualization.

## Core Files
- `geometry_client.py` - Basic gRPC client with core functionality
- `enhanced_geometry_client.py` - Enhanced client with async import support
- `pythonocc_viewer.py` - Professional PythonOCC viewer with AIS rendering

## Requirements
```bash
# Install with conda (recommended)
conda install -c conda-forge pythonocc-core
conda install grpcio grpcio-tools protobuf numpy

# Or with pip
pip install -r requirements.txt
```

## Usage

### 1. Generate Proto Files
```bash
cd clients/python
python setup.py
```

### 2. Start Server
```bash
./build/debug/GeometryServer.exe
```

### 3. Run PythonOCC Client
```bash
cd clients/python
python pythonocc_viewer.py
```

## Features
- **Professional AIS Rendering**: High-quality 3D visualization with materials and transparency
- **Async Model Import**: Non-blocking import of STEP, BREP, STL, IGES files  
- **Server Integration**: Full gRPC communication with GeometryServer
- **Multi-Client Session**: Client identification and session isolation
- **Disconnect Notifications**: Automatic server cleanup when client exits
- **Interactive Controls**: Mouse-based rotation, pan, and zoom

## PythonOCC AIS Rendering
The client uses PythonOCC's AIS (Application Interactive Services) for professional visualization:

```python
# Create AIS shape with professional appearance
ais_shape = AIS_ColoredShape(shape)
ais_shape.SetColor(Quantity_Color(Quantity_NOC_RED))
ais_shape.SetMaterial(Graphic3d_NOM_BRASS)
ais_shape.SetTransparency(0.2)
```

### Supported Materials
- Brass, Bronze, Copper
- Gold, Silver, Pewter
- Plastic, Plaster

## Controls
- **Left Mouse**: Rotate view
- **Middle Mouse**: Pan view  
- **Right Mouse**: Zoom
- **Menu Bar**: Server operations and demos

## Architecture
```
GeometryServer (C++ OCCT) ──┐
                           │ gRPC with client-id metadata
Python Client A ───────────┤ Session isolation
Python Client B ───────────┤ Independent shape storage  
C++ ImGui Client ──────────┘ Thread-safe operations
    ↓
PythonOCC AIS Display
```

Each client has isolated session with independent shape storage. Automatic cleanup on disconnect.