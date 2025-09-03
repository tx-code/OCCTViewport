cmake_minimum_required(VERSION 3.15)

# Project configuration
project(OcctImgui LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})

# Find packages
find_package(glfw3 REQUIRED)
find_package(OpenCASCADE REQUIRED)
find_package(spdlog REQUIRED)
find_package(imgui REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# Generate gRPC and Protocol Buffer files
set(PROTO_FILES
    proto/geometry_types.proto
    proto/geometry_service.proto
)

# Generated files directory
set(GENERATED_PROTOBUF_PATH ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH})

set(GENERATED_FILES)
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    
    # Generate .pb.cc, .pb.h, .grpc.pb.cc, .grpc.pb.h files
    set(PROTO_SOURCES "${GENERATED_PROTOBUF_PATH}/${PROTO_NAME}.pb.cc")
    set(PROTO_HEADERS "${GENERATED_PROTOBUF_PATH}/${PROTO_NAME}.pb.h")
    set(GRPC_SOURCES "${GENERATED_PROTOBUF_PATH}/${PROTO_NAME}.grpc.pb.cc")
    set(GRPC_HEADERS "${GENERATED_PROTOBUF_PATH}/${PROTO_NAME}.grpc.pb.h")
    
    list(APPEND GENERATED_FILES ${PROTO_SOURCES} ${PROTO_HEADERS} ${GRPC_SOURCES} ${GRPC_HEADERS})
    
    add_custom_command(
        OUTPUT ${PROTO_SOURCES} ${PROTO_HEADERS} ${GRPC_SOURCES} ${GRPC_HEADERS}
        COMMAND $<TARGET_FILE:protobuf::protoc>
        ARGS --grpc_out=${GENERATED_PROTOBUF_PATH}
             --cpp_out=${GENERATED_PROTOBUF_PATH}
             -I=${CMAKE_SOURCE_DIR}/proto
             --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
             ${CMAKE_SOURCE_DIR}/${PROTO_FILE}
        DEPENDS ${CMAKE_SOURCE_DIR}/${PROTO_FILE}
    )
endforeach()

# Create gRPC library
add_library(grpc_proto_lib ${GENERATED_FILES})
target_link_libraries(grpc_proto_lib 
    PUBLIC 
    protobuf::libprotobuf
    gRPC::gpr 
    gRPC::grpc 
    gRPC::grpc++
    gRPC::grpc++_alts
)
target_include_directories(grpc_proto_lib PUBLIC ${GENERATED_PROTOBUF_PATH})

# Common source files
file(GLOB COMMON_SOURCES 
    "src/common/*.cpp" 
    "src/common/*.h"
)

# Geometry Service (Server) executable
file(GLOB SERVER_SOURCES 
    "src/server/*.cpp" 
    "src/server/*.h"
)

add_executable(GeometryService 
    src/apps/server/server_main.cpp
    ${SERVER_SOURCES}
    ${COMMON_SOURCES}
)

target_link_libraries(GeometryService
    PRIVATE
    grpc_proto_lib
    ${OpenCASCADE_LIBRARIES}
    spdlog::spdlog
)

target_compile_options(GeometryService PRIVATE /utf-8)

# Note: RenderClient temporarily disabled due to glm dependency

# Core sources (3D rendering)
file(GLOB CORE_SOURCES 
    "src/core/GlfwOcctView.cpp" 
    "src/core/GlfwOcctView.h"
)

# Client gRPC sources 
file(GLOB GRPC_CLIENT_SOURCES 
    "src/client/grpc/geometry_client.cpp" 
    "src/client/grpc/geometry_client.h"
)

# Client render sources
file(GLOB RENDER_CLIENT_SOURCES 
    "src/client/render/*.cpp" 
    "src/client/render/*.h"
)

# Main OCCT ImGui Application with gRPC integration
add_executable(OcctImgui
    src/apps/grpc_viewer/main.cpp
    ${CORE_SOURCES}
    ${COMMON_SOURCES}
    ${GRPC_CLIENT_SOURCES}
)

target_link_libraries(OcctImgui
    PRIVATE
    grpc_proto_lib
    ${OpenCASCADE_LIBRARIES}
    glfw
    spdlog::spdlog
    imgui::imgui
)

target_compile_options(OcctImgui PRIVATE /utf-8)

# Test client executable for debugging
add_executable(TestClient
    tests/test_client.cpp
    ${COMMON_SOURCES}
    ${GRPC_CLIENT_SOURCES}
)

target_link_libraries(TestClient
    PRIVATE
    grpc_proto_lib
    spdlog::spdlog
)

target_compile_options(TestClient PRIVATE /utf-8)

# Create directories for source organization
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/common)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/server)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/client)