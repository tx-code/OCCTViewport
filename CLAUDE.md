# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Distributed OCCT (OpenCASCADE) + ImGui application with gRPC-based client-server architecture for 3D geometry processing and visualization. The architecture follows a hybrid model where both client and server use OCCT.

### Main Components
- **GeometryServer**: gRPC server handling OCCT geometry operations (port 50051)
- **OcctViewer**: Client with 3D rendering and ImGui interface (can run standalone)
- **OcctImgui_ServiceTests**: Comprehensive test suite (17 non-UI tests)

## Build Commands

```bash
# Debug build
cmake --preset debug
cmake --build build/debug --config Debug

# Release build
cmake --preset release
cmake --build build/release --config Release

# Run tests
./build/debug/OcctImgui_ServiceTests.exe

# Run specific test suite
./build/debug/OcctImgui_ServiceTests.exe --gtest_filter="ModelImportTest.*"

# List all tests
./build/debug/OcctImgui_ServiceTests.exe --gtest_list_tests
```

### Build Output Locations
- **Executables**: `build/debug/` (directly in root, not in bin/)
- **Test executable**: `build/debug/OcctImgui_ServiceTests.exe`
- **Proto generated files**: `build/debug/generated/`

## Running the Application

```bash
# Standalone mode (no server needed)
./build/debug/OcctViewer.exe

# Distributed mode
# Terminal 1:
./build/debug/GeometryServer.exe

# Terminal 2:
./build/debug/OcctViewer.exe
```

## Architecture

### Client-Server Communication Flow
1. Client sends gRPC request to server
2. Server processes with OCCT and returns mesh data
3. Client renders mesh in off-screen framebuffer
4. Framebuffer texture displayed in ImGui viewport

### Key Implementation Files

**Server Side:**
- `src/server/geometry_service_impl.cpp` - All OCCT operations, model import/export
- Supports unified import/export for STEP, BREP, STL, IGES formats

**Client Side:**
- `src/client/occt/OcctRenderClient.cpp` - Rendering and UI, async import with progress tracking
- `src/client/grpc/geometry_client.cpp` - gRPC communication layer
- Features multi-file import via NFD (Native File Dialog)

**Protocol Buffers:**
- `proto/geometry_service.proto` - Unified ImportModelFile/ExportModelFile services
- `proto/geometry_types.proto` - Core data types only (simplified by 60%)

### Model Import/Export Implementation

The project uses OCCT 7.9's unified DE (Data Exchange) package:

```cpp
// Server-side import (auto-detects format)
Handle(DE_Wrapper) de_wrapper = new DE_Wrapper();
if (!de_wrapper->Read(file_path, shape)) {
    // Handle error
}

// STEP export
STEPControl_Writer writer;
writer.Transfer(shape, STEPControl_AsIs);
writer.Write(output_path);

// BREP export  
BRepTools::Write(shape, stream);
```

### Async Import with Progress Tracking

Client supports multiple concurrent imports with progress bars:
```cpp
struct ImportTask {
    std::string id;
    std::future<GeometryClient::ModelImportResult> future;
    std::atomic<float> progress{0.0f};
    // ...
};
```

## Testing

### Test Structure (17 tests total)
- **SimpleGrpcTest** (5 tests): Core gRPC functionality
- **ModelImportTest** (6 tests): Import/export with auto-detection
- **RealModelsTest** (6 tests): Tests with real model files in `tests/test_data/models/`

### Test Data
The `tests/test_data/models/` directory contains test models:
- BREP: `3boxes.brep`, `simple_wall.brep`
- STEP: `179_synthetic_case.stp`
- STL: `bolt.stl`
- IGES: `bearing.igs`

## Development Guidelines

### Adding New Geometry Operations
1. Add RPC method to `proto/geometry_service.proto`
2. Implement in `geometry_service_impl.cpp`
3. Add client method in `geometry_client.cpp`
4. Update UI in `OcctRenderClient.cpp` if needed

### Connection Management
- Auto-reconnect every 10 seconds when disconnected
- Async connection to prevent UI freezing
- Connection states: Connected, Connecting, Disconnected, Error

### Performance Optimizations
- gRPC streaming for large mesh datasets
- GetSystemInfo cached at 1-second intervals
- Multiple concurrent file imports supported

## Important Notes

- Only one GeometryServer can run at a time (port 50051)
- UI tests have been removed - focus is on non-UI testing only  
- Client can run standalone without server (limited functionality)
- All model import/export uses unified OCCT DE package (no format-specific methods)
- References directory contains reference code (read-only, not for modification)