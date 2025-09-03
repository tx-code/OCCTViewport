# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a distributed OCCT (OpenCASCADE) + GLFW + ImGui application with off-screen rendering and gRPC-based client-server architecture. It's a fork of the original OcctImgui project, modified to use off-screen framebuffer rendering and distributed computing for geometry processing. The architecture follows a hybrid model where both client and server use OCCT (not a pure thin client).

The project consists of three main executables:
1. **GeometryServer** - gRPC server handling OCCT geometry operations (port 50051)
2. **OcctViewer** - Client application with OCCT-based 3D rendering and ImGui interface (can run standalone)
3. **GrpcTestClient** - Command-line tool for testing gRPC communication

## Build System

### Prerequisites
- CMake 3.15+
- vcpkg package manager
- Visual Studio (MSVC) compiler
- C++20 standard

### Required Dependencies (via vcpkg)
```bash
vcpkg install imgui glfw3 opencascade spdlog protobuf grpc
```

### Build Commands
The project uses CMake with user presets for configuration:

**Debug Build:**
```bash
cmake --preset debug
cmake --build build/debug --config Debug
```

**Release Build:**
```bash
cmake --preset release  
cmake --build build/release --config Release
```

**Build Output:**
- Executables: `build/debug/bin/Debug/` or `build/release/bin/Release/`
- Libraries: `build/debug/lib/Debug/` or `build/release/lib/Release/`

**Note:** If you get "could not load cache" error, reconfigure with `cmake --preset debug`

## Running the Application

### Standalone Mode
```bash
# Run viewer without server (local OCCT shapes only)
./build/debug/bin/Debug/OcctViewer.exe
```

### Distributed Mode
```bash
# Terminal 1: Start geometry server
./build/debug/bin/Debug/GeometryServer.exe

# Terminal 2: Start viewer client (auto-connects to localhost:50051)  
./build/debug/bin/Debug/OcctViewer.exe
```

### Testing gRPC Services
```bash
# Test with command-line client
./build/debug/bin/Debug/GrpcTestClient.exe
```

**Note:** Only one GeometryServer instance can run at a time (binds to port 50051). Client will continue in standalone mode if server unavailable.

## Architecture

### Distributed System Components

#### 1. GeometryServer (Server)
- **Location:** `src/apps/geometry_server/main.cpp`
- **Implementation:** `src/server/geometry_service_impl.h/.cpp`
- **Function:** Handles all OCCT geometry operations via gRPC
- **Port:** 50051 (configurable via command line)
- **Features:**
  - Primitive creation (Box, Cone, Sphere, Cylinder)
  - Shape management and transformations
  - Mesh data generation and streaming
  - Demo scene creation

#### 2. OcctViewer (Client)
- **Location:** `src/apps/occt_viewer/main.cpp`
- **Core Component:** `src/client/occt/OcctRenderClient.h/.cpp`
- **Function:** 3D rendering client with ImGui interface
- **Features:**
  - Off-screen framebuffer rendering
  - gRPC client for geometry operations
  - ImGui docking interface with gRPC Control Panel
  - GLFW window management with OpenGL 3.3 core

#### 3. GrpcTestClient
- **Location:** `tests/test_client.cpp`
- **Function:** Command-line tool for debugging gRPC communication
- **Output:** Detailed mesh data statistics and connectivity testing

### gRPC Protocol Buffers

#### Service Definition (`proto/geometry_service.proto`)
- **Primitives:** CreateBox, CreateCone, CreateSphere, CreateCylinder
- **Management:** DeleteShape, TransformShape, SetShapeColor
- **Data Retrieval:** GetMeshData, GetAllMeshes (streaming)
- **System:** ClearAll, GetSystemInfo, CreateDemoScene

#### Data Types (`proto/geometry_types.proto`)
- **Geometry:** Point3D, Vector3D, Transform (4x4 matrix)
- **Rendering:** MeshData with vertices, normals, indices
- **Properties:** Color (RGBA), BoundingBox, ShapeProperties

### Key Design Patterns

- **Client-Server Architecture**: Separates geometry computation from rendering
- **gRPC Streaming**: Efficient transmission of large mesh datasets
- **PIMPL Idiom**: OcctRenderClient uses `ViewInternal` struct for implementation hiding
- **Protected Inheritance**: Inherits from AIS_ViewController for OCCT integration
- **Off-screen Rendering**: Framebuffer-based rendering displayed in ImGui textures

### Directory Structure

```
src/
├── apps/
│   ├── occt_viewer/main.cpp    # OCCT viewer client entry point
│   └── geometry_server/main.cpp # Geometry server entry point
├── client/
│   ├── occt/
│   │   ├── OcctRenderClient.h/.cpp  # OCCT-based rendering client
├── client/
│   └── grpc/
│       ├── geometry_client.h/.cpp  # gRPC client implementation
├── server/
│   ├── geometry_service_impl.h/.cpp  # gRPC server implementation
├── common/
│   └── [shared utilities]
proto/
├── geometry_service.proto       # gRPC service definition
└── geometry_types.proto        # Protocol buffer data types
tests/
└── test_client.cpp             # gRPC testing utility
```

## Development Notes

### gRPC Integration

The client connects to the server automatically on startup. All geometry operations are performed remotely:

```cpp
// Create geometry via gRPC
GeometryClient client("localhost:50051");
std::string box_id = client.CreateBox(0, 0, 0, 2, 2, 2);

// Retrieve mesh data for rendering
auto meshes = client.GetAllMeshes();
```

### Adding New Geometry Operations

1. **Update Protocol Buffers:**
   - Add new RPC method to `geometry_service.proto`
   - Add request/response messages if needed

2. **Implement Server Side:**
   - Add method to `GeometryServiceImpl` class
   - Implement OCCT geometry creation logic

3. **Implement Client Side:**
   - Add method to `GeometryClient` class
   - Update UI controls in `GlfwOcctView` if needed

### ImGui gRPC Control Panel

The client includes a dedicated ImGui panel for gRPC operations:
- **Create Primitives**: Box, Cone, Sphere, Cylinder with random parameters
- **Scene Management**: Demo scene creation, clear all shapes
- **System Info**: Server version, active shapes count, OCCT version

All control panel operations include detailed logging for debugging.

### Rendering Pipeline

1. **Geometry Creation**: Client sends gRPC requests to server
2. **Mesh Generation**: Server computes triangulated meshes via OCCT
3. **Data Transmission**: Mesh data streamed back to client via gRPC
4. **Off-screen Rendering**: Client renders meshes to framebuffer
5. **Display**: Framebuffer texture displayed in ImGui viewport

### Error Handling and Logging

- **Server Logging**: Detailed gRPC request/response logging with spdlog
- **Client Logging**: Operation status and error reporting
- **Connection Management**: Automatic reconnection and error recovery
- **OpenGL Context**: Proper initialization order for Windows platform

### Performance Considerations

- **Streaming**: Large mesh datasets use gRPC streaming for efficiency
- **Caching**: Client caches mesh data to minimize server requests  
- **Network Optimization**: GetSystemInfo calls cached at 1-second intervals to reduce overhead from 60+ calls/second
- **Memory Management**: Proper cleanup of OCCT objects and gRPC resources

## Development Notes

### Connection Management
- Client implements async connection using `std::async` to prevent UI freezing during server connection attempts  
- Auto-reconnect attempts every 10 seconds when disconnected with proper timeout handling
- Connection status displayed in gRPC Control Panel (Connected, Connecting, Disconnected, Error states)
- Proper null pointer checks prevent crashes when accessing geometry client after disconnect

### Adding AIS Objects
Use `OcctRenderClient::addAisObject()` to add 3D objects to the scene.

### Viewport Interaction
- Mouse position adjustment handled by `adjustMousePosition()` 
- Supports standard OCCT navigation (pan, zoom, rotate)
- Event callbacks route through static GLFW functions to instance methods

### gRPC Service Extensions
To add new geometry operations:
1. Define message types in `proto/geometry_types.proto`
2. Add service methods in `proto/geometry_service.proto`  
3. Implement server-side in `geometry_service_impl.cpp`
4. Add client methods in `geometry_client.cpp`
5. Regenerate protobuf/gRPC code by rebuilding

### Rendering Pipeline
1. Off-screen rendering to framebuffer (FBO)
2. ImGui displays framebuffer texture in viewport
3. GLFW handles window management and input events
4. OCCT components initialized in order: rendering system → V3d viewer → AIS context → visual settings → framebuffer

## Common Issues

### Connection Failures
- Ensure GeometryServer is running on port 50051
- Check firewall settings if connecting across network
- Client will continue in standalone mode if server unavailable

### Build Errors
- If "could not load cache" error: run `cmake --preset debug` to reconfigure
- Ensure all vcpkg dependencies are installed
- Check that C++20 standard is enabled