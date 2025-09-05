#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <fstream>

#include "server/geometry_service_impl.h"
#include "geometry_service.pb.h"

// Test fixture for model import operations
class ModelImportTest : public ::testing::Test {
protected:
    void SetUp() override {
        service_ = std::make_unique<GeometryServiceImpl>();
        spdlog::set_level(spdlog::level::info);
        
        // Create test directory
        system("mkdir -p test_models 2>nul");
        
        // Create test files
        CreateTestSTEPFile();
        CreateTestSTLFile();
        CreateTestOBJFile();
    }
    
    void TearDown() override {
        // Clean up test files
        system("rm -rf test_models 2>nul");
    }
    
    void CreateTestSTEPFile() {
        // We don't need to create a STEP file since we'll use the existing one
        // This function is kept empty for compatibility
    }
    
    void CreateTestSTLFile() {
        std::ofstream file("test_models/test.stl");
        file << "solid test\n";
        file << "  facet normal 0 0 1\n";
        file << "    outer loop\n";
        file << "      vertex 0 0 0\n";
        file << "      vertex 1 0 0\n";
        file << "      vertex 0 1 0\n";
        file << "    endloop\n";
        file << "  endfacet\n";
        file << "endsolid test\n";
        file.close();
    }
    
    void CreateTestOBJFile() {
        std::ofstream file("test_models/test.obj");
        file << "# Test OBJ file\n";
        file << "v 0.0 0.0 0.0\n";
        file << "v 1.0 0.0 0.0\n";
        file << "v 0.0 1.0 0.0\n";
        file << "v 0.0 0.0 1.0\n";
        file << "f 1 2 3\n";
        file << "f 1 2 4\n";
        file << "f 1 3 4\n";
        file << "f 2 3 4\n";
        file.close();
    }
    
    std::unique_ptr<GeometryServiceImpl> service_;
    grpc::ServerContext context_;
};

// Test unified model import with auto-detection
TEST_F(ModelImportTest, AutoDetectFormatImport) {
    geometry::ModelFileRequest request;
    request.set_file_path("test_models/test.stl");
    request.mutable_options()->set_auto_detect_format(true);
    request.mutable_options()->set_import_colors(true);
    request.mutable_options()->set_import_names(true);
    
    geometry::ModelImportResponse response;
    auto status = service_->ImportModelFile(&context_, &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(response.success());
    EXPECT_EQ(response.detected_format(), "STL");
    EXPECT_FALSE(response.shape_ids().empty());
}

// Test import with forced format
TEST_F(ModelImportTest, ForcedFormatImport) {
    geometry::ModelFileRequest request;
    request.set_file_path("test_models/test.stl");
    request.mutable_options()->set_auto_detect_format(false);
    request.mutable_options()->set_force_format("STL");
    
    geometry::ModelImportResponse response;
    auto status = service_->ImportModelFile(&context_, &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(response.success());
    EXPECT_EQ(response.detected_format(), "STL");
}

// Test import with invalid file
TEST_F(ModelImportTest, InvalidFileImport) {
    geometry::ModelFileRequest request;
    request.set_file_path("test_models/nonexistent.step");
    request.mutable_options()->set_auto_detect_format(true);
    
    geometry::ModelImportResponse response;
    auto status = service_->ImportModelFile(&context_, &request, &response);
    
    EXPECT_TRUE(status.ok()); // gRPC call should succeed
    EXPECT_FALSE(response.success()); // But import should fail
    EXPECT_FALSE(response.message().empty());
}

// Test concurrent imports
TEST_F(ModelImportTest, ConcurrentImports) {
    std::vector<std::future<bool>> futures;
    
    // Launch multiple import tasks
    for (int i = 0; i < 3; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            grpc::ServerContext ctx;
            geometry::ModelFileRequest request;
            
            // Import different file types
            if (i == 0) {
                request.set_file_path("test_models/test.stl");
            } else if (i == 1) {
                request.set_file_path("test_models/test.obj");
            } else {
                // Use the existing STEP test file instead of creating one
                request.set_file_path("tests/test_data/models/179_synthetic_case.stp");
            }
            
            request.mutable_options()->set_auto_detect_format(true);
            
            geometry::ModelImportResponse response;
            auto status = service_->ImportModelFile(&ctx, &request, &response);
            
            // Log details for debugging
            if (!response.success()) {
                spdlog::warn("Import failed for file {}: {}", 
                    request.file_path(), response.message());
            }
            
            return status.ok() && response.success();
        }));
    }
    
    // Wait for all imports to complete
    bool all_success = true;
    for (auto& future : futures) {
        all_success = all_success && future.get();
    }
    
    EXPECT_TRUE(all_success);
}

// Test export functionality
TEST_F(ModelImportTest, ExportImportedModel) {
    // First import a model
    geometry::ModelFileRequest import_request;
    import_request.set_file_path("test_models/test.stl");
    import_request.mutable_options()->set_auto_detect_format(true);
    
    geometry::ModelImportResponse import_response;
    auto import_status = service_->ImportModelFile(&context_, &import_request, &import_response);
    
    ASSERT_TRUE(import_status.ok());
    ASSERT_TRUE(import_response.success());
    ASSERT_FALSE(import_response.shape_ids().empty());
    
    // Now export it to a different format
    geometry::ModelExportRequest export_request;
    for (const auto& shape_id : import_response.shape_ids()) {
        export_request.add_shape_ids(shape_id);
    }
    export_request.mutable_options()->set_format("STEP");
    export_request.mutable_options()->set_export_colors(true);
    export_request.mutable_options()->set_export_names(true);
    
    geometry::ModelFileResponse export_response;
    auto export_status = service_->ExportModelFile(&context_, &export_request, &export_response);
    
    EXPECT_TRUE(export_status.ok());
    EXPECT_TRUE(export_response.success());
    EXPECT_FALSE(export_response.model_data().empty());
    EXPECT_EQ(export_response.file_info().format(), "STEP");
}

// Test mesh precision option
TEST_F(ModelImportTest, ImportWithCustomPrecision) {
    geometry::ModelFileRequest request;
    request.set_file_path("test_models/test.stl");
    request.mutable_options()->set_auto_detect_format(true);
    request.mutable_options()->set_precision(0.001); // High precision
    
    geometry::ModelImportResponse response;
    auto status = service_->ImportModelFile(&context_, &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(response.success());
    
    // Get mesh data to verify precision
    if (!response.shape_ids().empty()) {
        geometry::ShapeRequest mesh_request;
        mesh_request.set_shape_id(response.shape_ids(0));
        
        geometry::MeshData mesh_response;
        grpc::ServerContext mesh_ctx;
        auto mesh_status = service_->GetMeshData(&mesh_ctx, &mesh_request, &mesh_response);
        
        EXPECT_TRUE(mesh_status.ok());
        EXPECT_FALSE(mesh_response.vertices().empty());
    }
}