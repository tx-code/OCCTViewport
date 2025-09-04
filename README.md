# OcctImgui

Distributed OpenCASCADE geometry processing with gRPC and ImGui rendering.

This is a fork of [eryar/OcctImgui](https://github.com/eryar/OcctImgui), redesigned with:
- **Distributed Architecture**: Client-server separation via gRPC
- **Off-screen Rendering**: Framebuffer-based 3D rendering
- **Pure Thin Client**: UI controls disabled when disconnected from server
- **Modern Build System**: CMake presets with vcpkg integration

## Architecture

The project uses a distributed architecture with three main components:

### 1. GeometryServer
- gRPC server handling all OCCT geometry operations
- Provides services for creating, transforming, and managing 3D shapes
- Runs on port 50051 by default

### 2. OcctViewer
- Client application with OCCT-based 3D rendering
- ImGui interface with gRPC control panel
- Can run standalone with local shapes or connect to GeometryServer

### 3. Testing Framework
- **Service Tests**: GTest-based unit tests for gRPC services
- **UI Automation**: ImGui Test Engine for automated UI testing
- Validates pure thin client behavior (buttons disabled when disconnected)

## Dependencies

- **OpenCASCADE 7.9.0**: 3D CAD modeling kernel
- **Dear ImGui 1.91.9**: Immediate mode GUI library
- **GLFW 3**: Window and OpenGL context management
- **gRPC 1.71.0**: Remote procedure calls
- **Protobuf 29.3.0**: Protocol buffers for serialization
- **spdlog**: Fast C++ logging library
- **Google Test 1.16.0**: Unit testing framework

## Build

### Prerequisites

1. Install vcpkg package manager
2. Install required packages:
```bash
vcpkg install imgui[docking-experimental,glfw-binding,opengl3-binding] glfw3 opencascade spdlog protobuf grpc gtest
```

### Building with CMake Presets

```bash
# Debug build
cmake --preset debug
cmake --build build/debug --config Debug

# Release build  
cmake --preset release
cmake --build build/release --config Release
```

### Running the Application

#### Standalone Mode
```bash
./build/debug/bin/Debug/OcctViewer.exe
```

#### Distributed Mode
```bash
# Terminal 1: Start geometry server
./build/debug/bin/Debug/GeometryServer.exe

# Terminal 2: Start viewer client
./build/debug/bin/Debug/OcctViewer.exe
```

### Running Tests

#### Service Tests
```bash
./build/debug/OcctImgui_ServiceTests.exe
```

#### UI Automation Tests
```bash
./build/debug/bin/imgui_test_suite.exe -nogui -nopause occt_imgui
```

## Features

### gRPC Services
- **Primitives**: CreateBox, CreateCone, CreateSphere, CreateCylinder
- **Management**: DeleteShape, TransformShape, SetShapeColor
- **Data**: GetMeshData, GetAllMeshes (streaming)
- **System**: ClearAll, GetSystemInfo, CreateDemoScene

### UI Features
- Dockable ImGui windows
- gRPC control panel with connection status
- Real-time 3D viewport with OCCT rendering
- Performance metrics display
- Pure thin client mode (all operations disabled when disconnected)

## Technologies

### OpenCASCADE (OCCT)
Open CASCADE Technology is a software development platform for 3D CAD, CAM, CAE applications. It provides:
- 3D surface and solid modeling
- CAD data exchange
- Visualization capabilities

### Dear ImGui
Bloat-free immediate mode GUI library for C++. Features:
- Fast iteration and prototyping
- Renderer agnostic
- Perfect for tools and debug interfaces

### GLFW
Multi-platform library for OpenGL application development:
- Window and context creation
- Input handling
- Event processing

### gRPC
High-performance RPC framework:
- Protocol buffers for serialization
- Streaming support
- Cross-platform communication

## License

This project follows the original licensing terms. See individual component licenses:
- OpenCASCADE: LGPL v2.1
- Dear ImGui: MIT License
- GLFW: zlib/libpng License
- gRPC: Apache License 2.0