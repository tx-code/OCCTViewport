# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a distributed OCCT (OpenCASCADE) + GLFW + ImGui application implementing a client-server architecture similar to Autodesk Fusion 360. The geometry computation is separated from rendering through a gRPC-based distributed system, allowing for scalable 3D CAD visualization.

The project consists of three main executables:
1. **GeometryService** - gRPC server handling OCCT geometry operations
2. **OcctImgui** - Client application with 3D rendering and ImGui interface  
3. **TestClient** - Command-line tool for testing gRPC communication

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
cmake --build build --config Debug
```

**Release Build:**
```bash
cmake --preset release  
cmake --build build --config Release
```

### Running the Distributed System

**IMPORTANT:** Always start the GeometryService server before launching the client.

1. **Start the geometry server:**
```bash
./build/debug/bin/Debug/GeometryService.exe
```

2. **Launch the client application:**
```bash
./build/debug/bin/Debug/OcctImgui.exe
```

3. **Optional: Test gRPC communication:**
```bash
./build/debug/bin/Debug/TestClient.exe
```

**Note:** Only one GeometryService instance can run at a time (binds to port 50051).

## Architecture

### Distributed System Components

#### 1. GeometryService (Server)
- **Location:** `src/apps/server/server_main.cpp`
- **Implementation:** `src/server/geometry_service_impl.h/.cpp`
- **Function:** Handles all OCCT geometry operations via gRPC
- **Port:** 50051 (configurable via command line)
- **Features:**
  - Primitive creation (Box, Cone, Sphere, Cylinder)
  - Shape management and transformations
  - Mesh data generation and streaming
  - Demo scene creation

#### 2. OcctImgui (Client)
- **Location:** `src/apps/grpc_viewer/main.cpp`
- **Core Component:** `src/core/GlfwOcctView.h/.cpp`
- **Function:** 3D rendering client with ImGui interface
- **Features:**
  - Off-screen framebuffer rendering
  - gRPC client for geometry operations
  - ImGui docking interface with gRPC Control Panel
  - GLFW window management with OpenGL 3.3 core

#### 3. TestClient
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
- **PIMPL Idiom**: GlfwOcctView uses `ViewInternal` struct for implementation hiding
- **Protected Inheritance**: Inherits from AIS_ViewController for OCCT integration
- **Off-screen Rendering**: Framebuffer-based rendering displayed in ImGui textures

### Directory Structure

```
src/
├── apps/
│   ├── grpc_viewer/main.cpp     # Client application entry point
│   └── server/server_main.cpp   # Server application entry point
├── core/
│   ├── GlfwOcctView.h/.cpp     # Main 3D rendering component
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
- **Concurrent Operations**: Server can handle multiple geometry operations
- **Memory Management**: Proper cleanup of OCCT objects and gRPC resources