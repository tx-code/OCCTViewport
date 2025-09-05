####################################################################################
# Modern CMake build system for OcctImgui
# Distributed OCCT + gRPC geometry processing with ImGui rendering
####################################################################################

cmake_minimum_required(VERSION 3.21)

# Project declaration with modern metadata
project(OcctImgui
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "Distributed OCCT geometry processing with gRPC and ImGui rendering"
)

# Only set policies when we're the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Use modern CMake policies
    cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
    cmake_policy(SET CMP0079 NEW)  # target_link_libraries() allows use with targets in other directories
    
    # Enable folder organization in IDEs
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    
    # Enable testing support
    include(CTest)
    
    # Set default build type
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
            "Choose the type of build: Debug Release RelWithDebInfo MinSizeRel" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
    endif()
    
    # Enforce C++20 standard
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

####################################################################################
# Project Options
####################################################################################

option(BUILD_TESTING "Build unit tests" ON)
option(BUILD_EXAMPLES "Build example applications" ON)
# UI Test Engine removed - focusing on non-UI tests only

####################################################################################
# Dependencies
####################################################################################

# Core dependencies
find_package(PkgConfig REQUIRED)
find_package(OpenCASCADE REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(spdlog REQUIRED)
find_package(imgui CONFIG REQUIRED)
# Extract ImGui version from package or set manually
if(NOT DEFINED imgui_VERSION)
    set(imgui_VERSION "1.91.9")  # Version from vcpkg
endif()
# Try to find implot - optional for now
find_package(implot CONFIG QUIET)
# NFD (Native File Dialog) for file selection
find_package(nfd CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# Testing dependencies (conditional)
if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    if (NOT GTest_FOUND)
        message(FATAL_ERROR "Google Test not found. Please install Google Test via vcpkg")
    endif()
    message(STATUS "Found Google Test ${GTest_VERSION}")
endif()

# UI Test Engine removed - focusing on non-UI tests only

####################################################################################
# Protocol Buffers & gRPC Generation
####################################################################################

# Proto files
set(PROTO_FILES
    proto/geometry_types.proto
    proto/geometry_service.proto
)

# Generated files directory
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Generate protobuf and gRPC files
set(GENERATED_SOURCES)
set(GENERATED_HEADERS)

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    
    set(PROTO_SOURCES "${GENERATED_DIR}/${PROTO_NAME}.pb.cc")
    set(PROTO_HEADERS "${GENERATED_DIR}/${PROTO_NAME}.pb.h")
    set(GRPC_SOURCES "${GENERATED_DIR}/${PROTO_NAME}.grpc.pb.cc")
    set(GRPC_HEADERS "${GENERATED_DIR}/${PROTO_NAME}.grpc.pb.h")
    
    list(APPEND GENERATED_SOURCES ${PROTO_SOURCES} ${GRPC_SOURCES})
    list(APPEND GENERATED_HEADERS ${PROTO_HEADERS} ${GRPC_HEADERS})
    
    add_custom_command(
        OUTPUT ${PROTO_SOURCES} ${PROTO_HEADERS} ${GRPC_SOURCES} ${GRPC_HEADERS}
        COMMAND $<TARGET_FILE:protobuf::protoc>
        ARGS --grpc_out=${GENERATED_DIR}
             --cpp_out=${GENERATED_DIR}
             --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
             --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
             ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
        COMMENT "Generating protobuf and gRPC files for ${PROTO_NAME}"
        VERBATIM
    )
endforeach()

# Create proto library target
add_library(OcctImgui_Proto STATIC)
add_library(OcctImgui::Proto ALIAS OcctImgui_Proto)

target_sources(OcctImgui_Proto
    PRIVATE
        ${GENERATED_SOURCES}
        ${GENERATED_HEADERS}  # For IDE visibility
)

target_include_directories(OcctImgui_Proto
    PUBLIC
        $<BUILD_INTERFACE:${GENERATED_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(OcctImgui_Proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

####################################################################################
# Core Libraries
####################################################################################

# Common utilities library
add_library(OcctImgui_Common STATIC)
add_library(OcctImgui::Common ALIAS OcctImgui_Common)

target_sources(OcctImgui_Common
    PRIVATE
        src/common/utils.cpp
        src/common/utils.h
)

target_include_directories(OcctImgui_Common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(OcctImgui_Common
    PUBLIC
        spdlog::spdlog
        ${OpenCASCADE_LIBRARIES}
)

# gRPC Client library
add_library(OcctImgui_GrpcClient STATIC)
add_library(OcctImgui::GrpcClient ALIAS OcctImgui_GrpcClient)

target_sources(OcctImgui_GrpcClient
    PRIVATE
        src/client/grpc/geometry_client.cpp
        src/client/grpc/geometry_client.h
)

target_include_directories(OcctImgui_GrpcClient
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(OcctImgui_GrpcClient
    PUBLIC
        OcctImgui::Proto
        OcctImgui::Common
        spdlog::spdlog
)

# OCCT Client library  
add_library(OcctImgui_OcctClient STATIC)
add_library(OcctImgui::OcctClient ALIAS OcctImgui_OcctClient)

target_sources(OcctImgui_OcctClient
    PRIVATE
        src/client/occt/OcctRenderClient.cpp
        src/client/occt/OcctRenderClient.h
        src/client/ui/grpc_performance_panel.cpp
        src/client/ui/grpc_performance_panel.h
        src/client/ui/console_panel.cpp
        src/client/ui/console_panel.h
        src/client/ui/logger_manager.cpp
        src/client/ui/logger_manager.h
        src/client/ui/console_sink.h
        src/client/ui/console_sink_impl.h
)

target_include_directories(OcctImgui_OcctClient
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(OcctImgui_OcctClient
    PUBLIC
        OcctImgui::Common
        OcctImgui::Proto
        ${OpenCASCADE_LIBRARIES}
        glfw
        OpenGL::GL
        imgui::imgui
        nfd::nfd
)

# Pass version information as compile definitions
target_compile_definitions(OcctImgui_OcctClient
    PRIVATE
        OCCT_VERSION="${OpenCASCADE_VERSION}"
        IMGUI_VERSION="${imgui_VERSION}"
)

# Add implot if available
if(implot_FOUND)
    target_link_libraries(OcctImgui_OcctClient PRIVATE implot::implot)
    target_compile_definitions(OcctImgui_OcctClient PRIVATE IMPLOT_VERSION)
    message(STATUS "ImPlot found and linked to OcctClient")
else()
    message(STATUS "ImPlot not found - performance panel will use text display")
endif()

# Geometry Server library
add_library(OcctImgui_Server STATIC)
add_library(OcctImgui::Server ALIAS OcctImgui_Server)

target_sources(OcctImgui_Server
    PRIVATE
        src/server/geometry_service_impl.cpp
        src/server/geometry_service_impl.h
)

target_include_directories(OcctImgui_Server
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(OcctImgui_Server
    PUBLIC
        OcctImgui::Proto
        OcctImgui::Common
        ${OpenCASCADE_LIBRARIES}
)

# Pass version information as compile definitions
target_compile_definitions(OcctImgui_Server
    PRIVATE
        OCCT_VERSION="${OpenCASCADE_VERSION}"
)


####################################################################################
# Executables
####################################################################################

if(BUILD_EXAMPLES)
    # OCCT Viewer Application
    add_executable(OcctViewer)
    
    target_sources(OcctViewer
        PRIVATE
            src/apps/occt_viewer/main.cpp
    )
    
    target_link_libraries(OcctViewer
        PRIVATE
            OcctImgui::OcctClient
            OcctImgui::GrpcClient
    )
    
    # Geometry Server Application
    add_executable(GeometryServer)
    
    target_sources(GeometryServer
        PRIVATE
            src/apps/geometry_server/main.cpp
    )
    
    target_link_libraries(GeometryServer
        PRIVATE
            OcctImgui::Server
    )
    
endif()

####################################################################################
# Unit Tests
####################################################################################

if(BUILD_TESTING)
    enable_testing()
    
    # GTest-based service tests
    add_executable(OcctImgui_ServiceTests)
    
    target_sources(OcctImgui_ServiceTests
        PRIVATE
            tests/grpc/simple_grpc_test.cpp
            tests/grpc/model_import_test.cpp
            tests/grpc/real_models_test.cpp
    )
    
    target_include_directories(OcctImgui_ServiceTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    
    target_link_libraries(OcctImgui_ServiceTests
        PRIVATE
            OcctImgui::Server
            OcctImgui::GrpcClient
            GTest::gtest_main
            GTest::gtest
    )
    
    
    # Configure Windows-specific options for service tests
    if(WIN32)
        target_compile_options(OcctImgui_ServiceTests 
            PRIVATE 
            /utf-8
            /bigobj
            /wd4251  # DLL interface warnings
            /wd4275  # DLL base class warnings
        )
        target_compile_definitions(OcctImgui_ServiceTests
            PRIVATE
            _CRT_SECURE_NO_WARNINGS
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )
        
    endif()
    
    # Add test discovery for GTest service tests
    include(GoogleTest)
    
    gtest_discover_tests(OcctImgui_ServiceTests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        XML_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_results"
        DISCOVERY_MODE PRE_TEST
    )
    
    message(STATUS "Configured gRPC Service Tests (OcctImgui_ServiceTests)")
else()
    message(STATUS "Unit testing disabled (BUILD_TESTING=OFF)")
endif()

####################################################################################
# Compiler Configuration
####################################################################################

# Apply modern C++20 features to all targets
function(configure_target target_name)
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    
    # Windows-specific configuration
    if(WIN32 AND MSVC)
        target_compile_options(${target_name} PRIVATE
            /utf-8
            /bigobj
            /wd4251  # DLL interface
            /wd4275  # DLL base class
        )
        target_compile_definitions(${target_name} PRIVATE
            _CRT_SECURE_NO_WARNINGS
            NOMINMAX 
            WIN32_LEAN_AND_MEAN
        )
    endif()
endfunction()

# Apply configuration manually to key targets
configure_target(OcctImgui_Proto)
configure_target(OcctImgui_Common)
configure_target(OcctImgui_GrpcClient)
configure_target(OcctImgui_OcctClient)
configure_target(OcctImgui_Server)

if(BUILD_EXAMPLES)
    configure_target(OcctViewer)
    configure_target(GeometryServer)
endif()

if(BUILD_TESTING)
    configure_target(OcctImgui_ServiceTests)
endif()

####################################################################################
# Summary
####################################################################################

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    message(STATUS "")
    message(STATUS "OcctImgui ${PROJECT_VERSION} Configuration Summary:")
    message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
    message(STATUS "  Unit tests:       ${BUILD_TESTING}")
    message(STATUS "  Examples:         ${BUILD_EXAMPLES}")
    message(STATUS "  UI Test Engine:   Removed (focusing on non-UI tests)")
    message(STATUS "  C++ Standard:     ${CMAKE_CXX_STANDARD}")
    message(STATUS "  Compiler:         ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "  OpenCASCADE:      ${OpenCASCADE_VERSION}")
    message(STATUS "  Dear ImGui:       ${imgui_VERSION}")
    message(STATUS "  gRPC:             ${gRPC_VERSION}")
    message(STATUS "  Protobuf:         ${Protobuf_VERSION}")
    if(BUILD_TESTING)
        message(STATUS "  Google Test:      ${GTest_VERSION}")
    endif()
    message(STATUS "")
    message(STATUS "Available targets:")
    message(STATUS "  OcctViewer        - OCCT rendering client with gRPC integration")
    message(STATUS "  GeometryServer    - gRPC geometry processing server")
    if(BUILD_TESTING)
        message(STATUS "  OcctImgui_ServiceTests - gRPC service unit tests")
    endif()
    message(STATUS "")
endif()