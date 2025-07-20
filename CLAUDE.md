# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an OCCT (OpenCASCADE) + GLFW + ImGui application that demonstrates 3D CAD visualization with off-screen rendering. It's a fork of the original OcctImgui project, modified to use off-screen rendering methods instead of direct window rendering.

## Build System

### Prerequisites
- CMake 3.15+
- vcpkg package manager
- Visual Studio (MSVC) compiler
- C++20 standard

### Required Dependencies (via vcpkg)
```bash
vcpkg install imgui glfw3 opencascade spdlog
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

The CMake configuration automatically sets:
- C++20 standard
- Output directories under `build/bin/` and `build/lib/`
- Links to OCCT modules: TKernel, TKMath, TKG2d, TKG3d, TKGeomBase, TKGeomAlgo, TKBRep, TKTopAlgo, TKPrim, TKMesh, TKService, TKOpenGl, TKV3d

## Architecture

### Core Components

1. **GlfwOcctView** (`GlfwOcctView.h/.cpp`) - Main application class
   - Inherits from `AIS_ViewController` (protected inheritance)
   - Manages OCCT rendering system, ImGui integration, and GLFW window events
   - Implements off-screen framebuffer rendering
   - Handles mouse/keyboard interaction for 3D viewport manipulation

2. **main.cpp** - Application entry point
   - Initializes GLFW window with OpenGL 3.3 core profile
   - Creates and runs GlfwOcctView instance
   - Handles GLFW lifecycle (creation/destruction)

### Key Design Patterns

- **PIMPL Idiom**: GlfwOcctView uses `ViewInternal` struct to hide implementation details
- **Protected Inheritance**: Inherits from AIS_ViewController to encapsulate OCCT's viewer functionality
- **Off-screen Rendering**: Uses framebuffers to render OCCT content, then displays in ImGui texture

### OCCT Integration

The application initializes OCCT components in this order:
1. OCCT rendering system
2. V3d viewer
3. AIS context for interactive objects
4. Visual settings and highlight styles
5. Off-screen framebuffer setup

## Development Notes

### Adding AIS Objects
Use `GlfwOcctView::addAisObject()` to add 3D objects to the scene.

### Viewport Interaction
- Mouse position adjustment handled by `adjustMousePosition()` 
- Supports standard OCCT navigation (pan, zoom, rotate)
- Event callbacks route through static GLFW functions to instance methods

### Rendering Pipeline
1. Off-screen rendering to framebuffer
2. ImGui displays framebuffer texture in viewport
3. GLFW handles window management and input events