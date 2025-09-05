#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <chrono>

#include "server/geometry_service_impl.h"
#include "geometry_service.pb.h"

namespace fs = std::filesystem;

// Test with real model files from E:\Model directory
class RealModelsTest : public ::testing::Test {
protected:
    void SetUp() override {
        service_ = std::make_unique<GeometryServiceImpl>();
        spdlog::set_level(spdlog::level::info);
    }
    
    // Helper function to test model import/export roundtrip
    bool TestModelRoundtrip(const std::string& file_path) {
        grpc::ServerContext import_ctx;
        
        // Import the model
        geometry::ModelFileRequest import_request;
        import_request.set_file_path(file_path);
        import_request.mutable_options()->set_auto_detect_format(true);
        
        geometry::ModelImportResponse import_response;
        auto status = service_->ImportModelFile(&import_ctx, &import_request, &import_response);
        
        if (!status.ok() || !import_response.success()) {
            spdlog::error("Failed to import {}: {}", file_path, 
                         import_response.message());
            return false;
        }
        
        spdlog::info("Successfully imported {} with {} shapes", 
                    file_path, import_response.shape_ids_size());
        
        // Export the imported model to STEP format
        grpc::ServerContext export_ctx;
        geometry::ModelExportRequest export_request;
        for (const auto& shape_id : import_response.shape_ids()) {
            export_request.add_shape_ids(shape_id);
        }
        export_request.mutable_options()->set_format("STEP");
        
        geometry::ModelFileResponse export_response;
        status = service_->ExportModelFile(&export_ctx, &export_request, &export_response);
        
        if (!status.ok() || !export_response.success()) {
            spdlog::error("Failed to export {}: {}", file_path,
                         export_response.message());
            return false;
        }
        
        spdlog::info("Successfully exported to STEP format, size: {} bytes",
                    export_response.model_data().size());
        
        return true;
    }
    
    std::unique_ptr<GeometryServiceImpl> service_;
};

// Test BREP files from local test data directory
TEST_F(RealModelsTest, ImportBrepModels) {
    std::vector<std::string> brep_files = {
        "tests/test_data/models/3boxes.brep",
        "tests/test_data/models/simple_wall.brep"
    };
    
    int success_count = 0;
    int total_count = 0;
    
    for (const auto& file : brep_files) {
        if (!fs::exists(file)) {
            spdlog::warn("File not found: {}", file);
            continue;
        }
        
        total_count++;
        spdlog::info("Testing BREP file: {}", file);
        
        if (TestModelRoundtrip(file)) {
            success_count++;
            spdlog::info("✓ {} passed", fs::path(file).filename().string());
        } else {
            spdlog::error("✗ {} failed", fs::path(file).filename().string());
        }
    }
    
    EXPECT_GT(success_count, 0) << "At least one BREP file should import successfully";
    spdlog::info("BREP Import Results: {}/{} files passed", success_count, total_count);
}

// Test STEP files from local test data directory
TEST_F(RealModelsTest, ImportStepModels) {
    std::vector<std::string> step_files = {
        "tests/test_data/models/179_synthetic_case.stp"
    };
    
    int success_count = 0;
    int total_count = 0;
    
    for (const auto& file : step_files) {
        if (!fs::exists(file)) {
            spdlog::warn("File not found: {}", file);
            continue;
        }
        
        total_count++;
        spdlog::info("Testing STEP file: {}", file);
        
        if (TestModelRoundtrip(file)) {
            success_count++;
            spdlog::info("✓ {} passed", fs::path(file).filename().string());
        } else {
            spdlog::error("✗ {} failed", fs::path(file).filename().string());
        }
    }
    
    EXPECT_GT(success_count, 0) << "At least one STEP file should import successfully";
    spdlog::info("STEP Import Results: {}/{} files passed", success_count, total_count);
}

// Test STL file import
TEST_F(RealModelsTest, ImportStlModels) {
    std::vector<std::string> stl_files = {
        "tests/test_data/models/bolt.stl"
    };
    
    int success_count = 0;
    int total_count = 0;
    
    for (const auto& file : stl_files) {
        if (!fs::exists(file)) {
            spdlog::warn("File not found: {}", file);
            continue;
        }
        
        total_count++;
        spdlog::info("Testing STL file: {}", file);
        
        if (TestModelRoundtrip(file)) {
            success_count++;
            spdlog::info("✓ {} passed", fs::path(file).filename().string());
        } else {
            spdlog::error("✗ {} failed", fs::path(file).filename().string());
        }
    }
    
    EXPECT_GT(success_count, 0) << "At least one STL file should import successfully";
    spdlog::info("STL Import Results: {}/{} files passed", success_count, total_count);
}

// Test IGES file import
TEST_F(RealModelsTest, ImportIgesModels) {
    std::vector<std::string> iges_files = {
        "tests/test_data/models/bearing.igs"
    };
    
    int success_count = 0;
    int total_count = 0;
    
    for (const auto& file : iges_files) {
        if (!fs::exists(file)) {
            spdlog::warn("File not found: {}", file);
            continue;
        }
        
        total_count++;
        spdlog::info("Testing IGES file: {}", file);
        
        if (TestModelRoundtrip(file)) {
            success_count++;
            spdlog::info("✓ {} passed", fs::path(file).filename().string());
        } else {
            spdlog::error("✗ {} failed", fs::path(file).filename().string());
        }
    }
    
    EXPECT_GT(success_count, 0) << "At least one IGES file should import successfully";
    spdlog::info("IGES Import Results: {}/{} files passed", success_count, total_count);
}

// Test mesh generation for imported models
TEST_F(RealModelsTest, MeshGenerationForRealModels) {
    std::string test_file = "tests/test_data/models/3boxes.brep";
    
    if (!fs::exists(test_file)) {
        GTEST_SKIP() << "Test file not found: " << test_file;
    }
    
    // Import the model
    grpc::ServerContext import_ctx;
    geometry::ModelFileRequest import_request;
    import_request.set_file_path(test_file);
    import_request.mutable_options()->set_auto_detect_format(true);
    
    geometry::ModelImportResponse import_response;
    auto status = service_->ImportModelFile(&import_ctx, &import_request, &import_response);
    
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(import_response.success());
    ASSERT_GT(import_response.shape_ids_size(), 0);
    
    // Generate mesh for the imported shape
    for (const auto& shape_id : import_response.shape_ids()) {
        grpc::ServerContext mesh_ctx;
        geometry::ShapeRequest mesh_request;
        mesh_request.set_shape_id(shape_id);
        
        geometry::MeshData mesh_response;
        status = service_->GetMeshData(&mesh_ctx, &mesh_request, &mesh_response);
        
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(mesh_response.shape_id(), shape_id);
        EXPECT_GT(mesh_response.vertices_size(), 0);
        EXPECT_GT(mesh_response.indices_size(), 0);
        
        spdlog::info("Mesh for shape {}: {} vertices, {} indices",
                    shape_id, mesh_response.vertices_size(),
                    mesh_response.indices_size());
    }
}

// Performance test with different model formats
TEST_F(RealModelsTest, MultiFormatPerformance) {
    std::vector<std::string> test_files = {
        "tests/test_data/models/3boxes.brep",
        "tests/test_data/models/179_synthetic_case.stp",
        "tests/test_data/models/bolt.stl",
        "tests/test_data/models/bearing.igs"
    };
    
    for (const auto& file : test_files) {
        if (!fs::exists(file)) {
            spdlog::warn("Test file not found: {}", file);
            continue;
        }
        
        auto start = std::chrono::steady_clock::now();
        
        // Import the model
        grpc::ServerContext import_ctx;
        geometry::ModelFileRequest import_request;
        import_request.set_file_path(file);
        import_request.mutable_options()->set_auto_detect_format(true);
        
        geometry::ModelImportResponse import_response;
        auto status = service_->ImportModelFile(&import_ctx, &import_request, &import_response);
        
        auto import_time = std::chrono::steady_clock::now();
        
        if (!status.ok() || !import_response.success()) {
            spdlog::error("Failed to import {}", file);
            continue;
        }
        
        // Export the model
        grpc::ServerContext export_ctx;
        geometry::ModelExportRequest export_request;
        for (const auto& shape_id : import_response.shape_ids()) {
            export_request.add_shape_ids(shape_id);
        }
        export_request.mutable_options()->set_format("STEP");
        
        geometry::ModelFileResponse export_response;
        status = service_->ExportModelFile(&export_ctx, &export_request, &export_response);
        
        auto export_time = std::chrono::steady_clock::now();
        
        // Calculate times
        auto import_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            import_time - start).count();
        auto export_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            export_time - import_time).count();
        
        spdlog::info("Performance for {}:", fs::path(file).filename().string());
        spdlog::info("  Format: {}", import_response.detected_format());
        spdlog::info("  Import time: {} ms", import_ms);
        spdlog::info("  Export time: {} ms", export_ms);
        spdlog::info("  Total shapes: {}", import_response.shape_ids_size());
        
        // Performance expectations
        EXPECT_LT(import_ms, 5000) << "Import should complete within 5 seconds for " << file;
        EXPECT_LT(export_ms, 5000) << "Export should complete within 5 seconds for " << file;
    }
}