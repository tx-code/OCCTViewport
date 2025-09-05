# OcctImgui

分布式OpenCASCADE几何处理系统，支持gRPC多客户端连接和跨语言开发。

## 🏗️ 架构特点

- **多客户端会话管理**: 支持C++/Python客户端同时连接
- **会话隔离**: 每个客户端独立的几何体存储空间
- **跨语言支持**: C++ ImGui客户端 + Python PythonOCC客户端
- **现代构建系统**: CMake + vcpkg经典模式

## 🚀 快速开始

### 1️⃣ 构建服务端 (C++)
```bash
# 安装vcpkg依赖 (经典模式)
vcpkg install opencascade grpc protobuf spdlog gtest glfw3
vcpkg install imgui[docking-experimental,glfw-binding,opengl3-binding]

# 构建项目
cmake -B build/debug -S . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build build/debug --config Debug
```

### 2️⃣ 启动几何服务器
```bash
./build/debug/GeometryServer.exe
```
服务器运行在 `localhost:50051`

### 3️⃣ 运行客户端

#### C++ ImGui客户端
```bash
./build/debug/OcctViewer.exe
```

#### Python PythonOCC客户端
```bash
cd clients/python

# 安装Python依赖
pip install grpcio grpcio-tools protobuf numpy
conda install -c conda-forge pythonocc-core

# 生成protobuf文件
python setup.py

# 运行Python客户端
python pythonocc_viewer.py
```

## 🌐 多客户端测试
```bash
# 终端1: 启动服务器
./build/debug/GeometryServer.exe

# 终端2: C++客户端
./build/debug/OcctViewer.exe

# 终端3: Python客户端  
cd clients/python && python pythonocc_viewer.py

# 终端4: 另一个Python客户端
cd clients/python && python pythonocc_viewer.py
```

```
GeometryServer (C++ OCCT)
├── Session Management (线程安全)
├── Client A (C++ ImGui)    ──→ 独立几何体存储
├── Client B (Python)       ──→ 独立几何体存储
└── Client C (Python)       ──→ 独立几何体存储
```

**核心特性:**
- 🔐 **会话隔离**: 每个客户端有独立的几何体命名空间
- 🏷️ **客户端识别**: 通过gRPC metadata区分客户端
- 🔌 **断开通知**: 客户端主动通知服务器断开连接
- 🧹 **自动清理**: 5分钟超时后自动清理非活跃会话

## 🧪 测试功能
```bash
# 运行服务端测试 (17个测试用例)
./build/debug/OcctImgui_ServiceTests.exe

# 特定测试套件
./build/debug/OcctImgui_ServiceTests.exe --gtest_filter="ModelImportTest.*"
```

## 📋 支持的几何操作
- **几何体创建**: Box, Sphere, Cone, Cylinder
- **文件导入/导出**: STEP, BREP, STL, IGES (统一DE接口)
- **几何体管理**: 删除、变换、着色
- **会话管理**: 多客户端隔离、断开通知

## 💡 使用场景
- **多用户CAD服务**: 支持多个用户同时连接编辑
- **跨语言开发**: C++服务端 + Python/C++客户端
- **分布式几何处理**: 重型计算在服务端，客户端轻量化

## 🚨 重要提醒
- 只能运行一个GeometryServer实例 (端口50051)
- Python客户端需要先运行`setup.py`生成protobuf文件
- 推荐使用conda安装`pythonocc-core`以避免依赖问题

## 📄 许可证
遵循原项目许可条款，详见各组件许可证