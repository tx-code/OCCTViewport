#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <cstdio>

#include "server/geometry_service_impl.h"
#include "geometry_service.pb.h"

// Simplified server tests using unified import/export
class SimpleGrpcTest : public ::testing::Test {
protected:
    void SetUp() override {
        service_ = std::make_unique<GeometryServiceImpl>();
        spdlog::set_level(spdlog::level::info);
    }
    
    std::unique_ptr<GeometryServiceImpl> service_;
    grpc::ServerContext context_;
};

TEST_F(SimpleGrpcTest, CreateBoxShouldWork) {
    geometry::BoxRequest request;
    request.mutable_position()->set_x(0);
    request.mutable_position()->set_y(0);
    request.mutable_position()->set_z(0);
    request.set_width(5);
    request.set_height(5);
    request.set_depth(5);
    
    geometry::ShapeResponse response;
    auto status = service_->CreateBox(&context_, &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(response.shape_id().empty());
    EXPECT_TRUE(response.success());
}

TEST_F(SimpleGrpcTest, SystemInfoShouldWork) {
    geometry::EmptyRequest request;
    geometry::SystemInfoResponse response;
    
    auto status = service_->GetSystemInfo(&context_, &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(response.version().empty());
    EXPECT_FALSE(response.occt_version().empty());
    EXPECT_GE(response.active_shapes(), 0);
}

TEST_F(SimpleGrpcTest, ModelExportShouldWork) {
    // Create a box first
    geometry::BoxRequest box_request;
    box_request.mutable_position()->set_x(0);
    box_request.mutable_position()->set_y(0);
    box_request.mutable_position()->set_z(0);
    box_request.set_width(3);
    box_request.set_height(3);
    box_request.set_depth(3);
    
    geometry::ShapeResponse box_response;
    auto status = service_->CreateBox(&context_, &box_request, &box_response);
    ASSERT_TRUE(status.ok());
    
    // Export as STEP format
    geometry::ModelExportRequest export_request;
    export_request.add_shape_ids(box_response.shape_id());
    export_request.mutable_options()->set_format("STEP");
    export_request.mutable_options()->set_export_colors(true);
    export_request.mutable_options()->set_export_names(true);
    
    geometry::ModelFileResponse export_response;
    status = service_->ExportModelFile(&context_, &export_request, &export_response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(export_response.success());
    EXPECT_FALSE(export_response.model_data().empty());
    EXPECT_GT(export_response.model_data().size(), 50);
}

TEST_F(SimpleGrpcTest, ModelImportExportRoundtripShouldWork) {
    // Create and export shape
    geometry::BoxRequest box_request;
    box_request.mutable_position()->set_x(0);
    box_request.mutable_position()->set_y(0);
    box_request.mutable_position()->set_z(0);
    box_request.set_width(4);
    box_request.set_height(4);
    box_request.set_depth(4);
    
    geometry::ShapeResponse box_response;
    auto status = service_->CreateBox(&context_, &box_request, &box_response);
    ASSERT_TRUE(status.ok());
    
    // Export as STEP
    geometry::ModelExportRequest export_request;
    export_request.add_shape_ids(box_response.shape_id());
    export_request.mutable_options()->set_format("STEP");
    
    geometry::ModelFileResponse export_response;
    status = service_->ExportModelFile(&context_, &export_request, &export_response);
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(export_response.success());
    
    // Save exported data to temporary file
    std::string temp_file = "test_export.step";
    std::ofstream out(temp_file, std::ios::binary);
    out.write(export_response.model_data().data(), export_response.model_data().size());
    out.close();
    
    // Clear all shapes
    geometry::EmptyRequest clear_request;
    geometry::StatusResponse clear_response;
    status = service_->ClearAll(&context_, &clear_request, &clear_response);
    ASSERT_TRUE(status.ok());
    
    // Import back from file
    geometry::ModelFileRequest import_request;
    import_request.set_file_path(temp_file);
    import_request.mutable_options()->set_auto_detect_format(true);
    
    geometry::ModelImportResponse import_response;
    status = service_->ImportModelFile(&context_, &import_request, &import_response);
    
    EXPECT_TRUE(status.ok()) << "Import status: " << status.error_message();
    EXPECT_TRUE(import_response.success()) << "Import message: " << import_response.message();
    EXPECT_EQ(import_response.shape_ids_size(), 1);
    
    // Clean up temp file
    std::remove(temp_file.c_str());
}

TEST_F(SimpleGrpcTest, InvalidModelFileShouldFail) {
    // Create an invalid temporary file
    std::string temp_file = "invalid_model.step";
    std::ofstream out(temp_file);
    out << "This is not valid STEP data";
    out.close();
    
    geometry::ModelFileRequest request;
    request.set_file_path(temp_file);
    request.mutable_options()->set_auto_detect_format(true);
    
    geometry::ModelImportResponse response;
    auto status = service_->ImportModelFile(&context_, &request, &response);
    
    // Should return error status or success=false
    EXPECT_TRUE(!status.ok() || !response.success());
    
    // Clean up
    std::remove(temp_file.c_str());
}