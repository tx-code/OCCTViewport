// MIT License
//
// Copyright(c) 2025 Xing Tang <tang.xing1@outlook.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include "client/grpc/geometry_client.h"
#include "server/geometry_service_impl.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <thread>
#include <chrono>

// Integration test with automatic server management
class ServerManagedIntegrationTest : public ::testing::Test {
protected:
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<GeometryServiceImpl> service_;
    std::unique_ptr<GeometryClient> client_;
    std::thread server_thread_;
    std::string server_address_ = "localhost:50052";  // Use different port to avoid conflicts
    
    void SetUp() override {
        spdlog::set_level(spdlog::level::info);
        
        // Start server automatically
        StartServer();
        
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Create and connect client
        client_ = std::make_unique<GeometryClient>(server_address_);
        bool connected = client_->Connect();
        ASSERT_TRUE(connected) << "Failed to connect to test server";
    }
    
    void TearDown() override {
        if (client_) {
            client_->Disconnect();
        }
        
        StopServer();
    }
    
    // Helper methods for UI simulation
    bool SimulateButtonClick(const std::string& button_name) {
        if (!client_) return false;
        
        spdlog::info("Button '{}' clicked", button_name);
        
        if (button_name == "Create Box") {
            std::string id = client_->CreateBox(0, 0, 0, 2, 2, 2);
            return !id.empty();
        } else if (button_name == "Create Sphere") {
            std::string id = client_->CreateSphere(0, 0, 0, 1);
            return !id.empty();
        } else if (button_name == "Create Cone") {
            std::string id = client_->CreateCone(0, 0, 0, 1, 0.5, 2);
            return !id.empty();
        } else if (button_name == "Create Cylinder") {
            std::string id = client_->CreateCylinder(0, 0, 0, 1, 2);
            return !id.empty();
        } else if (button_name == "Clear All") {
            return client_->ClearAll();
        } else if (button_name == "Create Demo Scene") {
            return client_->CreateDemoScene();
        }
        
        return false;
    }
    
    bool IsButtonEnabled(const std::string& button_name) {
        // In a real UI integration test, this would check the actual button state
        // For now, simulate based on connection status
        return client_ && client_->IsConnected();
    }

private:
    void StartServer() {
        service_ = std::make_unique<GeometryServiceImpl>();
        
        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address_, grpc::InsecureServerCredentials());
        builder.RegisterService(service_.get());
        
        server_ = builder.BuildAndStart();
        ASSERT_TRUE(server_ != nullptr) << "Failed to start test server";
        
        spdlog::info("Test server started on {}", server_address_);
    }
    
    void StopServer() {
        if (server_) {
            server_->Shutdown();
            server_->Wait();
            spdlog::info("Test server stopped");
        }
    }
};

// Test that all geometry creation works with server
TEST_F(ServerManagedIntegrationTest, GeometryCreationShouldWork) {
    // Clear any existing shapes
    ASSERT_TRUE(SimulateButtonClick("Clear All"));
    
    // Test each geometry creation
    EXPECT_TRUE(SimulateButtonClick("Create Box"));
    EXPECT_TRUE(SimulateButtonClick("Create Sphere"));
    EXPECT_TRUE(SimulateButtonClick("Create Cone"));
    EXPECT_TRUE(SimulateButtonClick("Create Cylinder"));
    
    // Verify shapes were created
    auto info = client_->GetSystemInfo();
    EXPECT_EQ(info.active_shapes, 4);
    
    // Test demo scene
    ASSERT_TRUE(SimulateButtonClick("Clear All"));
    EXPECT_TRUE(SimulateButtonClick("Create Demo Scene"));
    
    // Demo scene should create multiple shapes
    info = client_->GetSystemInfo();
    EXPECT_GT(info.active_shapes, 1);
}

// Test rapid operations don't cause issues
TEST_F(ServerManagedIntegrationTest, RapidOperationsShouldWork) {
    ASSERT_TRUE(SimulateButtonClick("Clear All"));
    
    // Create shapes rapidly
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(SimulateButtonClick("Create Box"));
    }
    
    auto info = client_->GetSystemInfo();
    EXPECT_EQ(info.active_shapes, 5);
    
    // Clear all
    EXPECT_TRUE(SimulateButtonClick("Clear All"));
    info = client_->GetSystemInfo();
    EXPECT_EQ(info.active_shapes, 0);
}

// Test mesh data retrieval
TEST_F(ServerManagedIntegrationTest, MeshDataRetrievalShouldWork) {
    // Clear and create a box
    ASSERT_TRUE(SimulateButtonClick("Clear All"));
    EXPECT_TRUE(SimulateButtonClick("Create Box"));
    
    // Get mesh data
    auto meshes = client_->GetAllMeshes();
    EXPECT_EQ(meshes.size(), 1);
    
    const auto& mesh = meshes[0];
    EXPECT_GT(mesh.vertices.size(), 0);
    EXPECT_GT(mesh.indices.size(), 0);
    EXPECT_EQ(mesh.vertices.size() % 3, 0);  // Vertices should be in groups of 3 (x,y,z)
}

// Test connection stability
TEST_F(ServerManagedIntegrationTest, ConnectionStabilityShouldWork) {
    // Perform multiple operations to test connection stability
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(SimulateButtonClick("Clear All"));
        EXPECT_TRUE(SimulateButtonClick("Create Box"));
        
        auto info = client_->GetSystemInfo();
        EXPECT_EQ(info.active_shapes, 1);
    }
}