#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

#include "geometry_client.h"
// #include "render_view.h"  // We'll create this later

class SimpleRenderClient {
public:
    SimpleRenderClient() : geometry_client_("localhost:50051") {
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    }
    
    bool Initialize() {
        spdlog::info("SimpleRenderClient: Initializing...");
        
        // Initialize GLFW for future rendering
        if (!glfwInit()) {
            spdlog::error("SimpleRenderClient: Failed to initialize GLFW");
            return false;
        }
        
        // Connect to geometry server
        if (!geometry_client_.Connect()) {
            spdlog::error("SimpleRenderClient: Failed to connect to geometry server");
            glfwTerminate();
            return false;
        }
        
        return true;
    }
    
    void Run() {
        spdlog::info("SimpleRenderClient: Starting test sequence...");
        
        // Test system info
        auto system_info = geometry_client_.GetSystemInfo();
        spdlog::info("Server Info - Version: {}, Active Shapes: {}, OCCT: {}", 
                    system_info.version, system_info.active_shapes, system_info.occt_version);
        
        // Clear any existing shapes
        geometry_client_.ClearAll();
        
        // Test individual shape creation
        spdlog::info("Creating individual shapes...");
        
        std::string box_id = geometry_client_.CreateBox(0, 0, 0, 30, 30, 30, 0.8, 0.8, 0.8);
        if (!box_id.empty()) {
            spdlog::info("Created box with ID: {}", box_id);
        }
        
        std::string cone_id = geometry_client_.CreateCone(50, 50, 0, 20, 5, 40, 0.7, 0.9, 0.7);
        if (!cone_id.empty()) {
            spdlog::info("Created cone with ID: {}", cone_id);
        }
        
        // List all shapes
        auto shapes = geometry_client_.ListShapes();
        spdlog::info("Current shapes count: {}", shapes.size());
        for (const auto& shape_id : shapes) {
            spdlog::info("  - Shape ID: {}", shape_id);
        }
        
        // Test demo scene creation
        spdlog::info("Creating demo scene...");
        if (geometry_client_.CreateDemoScene()) {
            spdlog::info("Demo scene created successfully");
            
            // List shapes again
            shapes = geometry_client_.ListShapes();
            spdlog::info("Shapes after demo scene: {}", shapes.size());
            for (const auto& shape_id : shapes) {
                spdlog::info("  - Shape ID: {}", shape_id);
            }
        }
        
        // Test mesh data retrieval
        spdlog::info("Testing mesh data retrieval...");
        
        // Get individual mesh
        if (!shapes.empty()) {
            std::string test_shape_id = shapes[0];
            spdlog::info("Getting mesh data for shape: {}", test_shape_id);
            
            auto mesh_data = geometry_client_.GetMeshData(test_shape_id);
            if (!mesh_data.shape_id.empty()) {
                spdlog::info("Retrieved mesh: {} vertices, {} indices, color: ({}, {}, {}, {})",
                            mesh_data.vertices.size() / 3, mesh_data.indices.size(),
                            mesh_data.color[0], mesh_data.color[1], mesh_data.color[2], mesh_data.color[3]);
            }
        }
        
        // Get all meshes
        spdlog::info("Getting all mesh data...");
        auto all_meshes = geometry_client_.GetAllMeshes();
        spdlog::info("Retrieved {} meshes from server", all_meshes.size());
        
        for (const auto& mesh : all_meshes) {
            spdlog::info("  - Mesh {}: {} vertices, {} triangles",
                        mesh.shape_id, mesh.vertices.size() / 3, mesh.indices.size() / 3);
        }
        
        // Keep running for a short while to test server connection
        spdlog::info("Client running... Press Ctrl+C to exit");
        
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Periodically check connection
            if (geometry_client_.IsConnected()) {
                auto info = geometry_client_.GetSystemInfo();
                spdlog::info("Heartbeat: {} active shapes", info.active_shapes);
            } else {
                spdlog::error("Lost connection to server!");
                break;
            }
        }
        
        spdlog::info("SimpleRenderClient: Test completed");
    }
    
    void Shutdown() {
        spdlog::info("SimpleRenderClient: Shutting down...");
        geometry_client_.Disconnect();
        glfwTerminate();
    }

private:
    GeometryClient geometry_client_;
};

int main(int argc, char** argv) {
    spdlog::info("OCCT gRPC Render Client starting...");
    
    SimpleRenderClient client;
    
    if (!client.Initialize()) {
        spdlog::error("Failed to initialize client");
        return 1;
    }
    
    try {
        client.Run();
    } catch (const std::exception& e) {
        spdlog::error("Client error: {}", e.what());
        client.Shutdown();
        return 1;
    }
    
    client.Shutdown();
    return 0;
}