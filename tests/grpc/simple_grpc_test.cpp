#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "server/geometry_service_impl.h"
#include "geometry_service.pb.h"

// 简化的服务端测试
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

TEST_F(SimpleGrpcTest, BrepExportShouldWork) {
    // 先创建一个形状
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
    
    // 导出BREP
    geometry::BrepExportRequest export_request;
    export_request.add_shape_ids(box_response.shape_id());
    
    geometry::BrepFileResponse export_response;
    status = service_->ExportBrepFile(&context_, &export_request, &export_response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(export_response.success());
    EXPECT_FALSE(export_response.brep_data().empty());
    EXPECT_GT(export_response.brep_data().size(), 50);
}

TEST_F(SimpleGrpcTest, BrepImportFromExportedDataShouldWork) {
    // 创建并导出形状
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
    
    geometry::BrepExportRequest export_request;
    export_request.add_shape_ids(box_response.shape_id());
    
    geometry::BrepFileResponse export_response;
    status = service_->ExportBrepFile(&context_, &export_request, &export_response);
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(export_response.success());
    
    // 清除所有形状
    geometry::EmptyRequest clear_request;
    geometry::StatusResponse clear_response;
    status = service_->ClearAll(&context_, &clear_request, &clear_response);
    ASSERT_TRUE(status.ok());
    
    // 从导出的数据导入
    geometry::BrepDataRequest import_request;
    import_request.set_brep_data(export_response.brep_data());
    import_request.set_filename("test_import.brep");
    
    geometry::BrepImportResponse import_response;
    status = service_->LoadBrepFromData(&context_, &import_request, &import_response);
    
    EXPECT_TRUE(status.ok()) << "Import status: " << status.error_message();
    EXPECT_TRUE(import_response.success()) << "Import message: " << import_response.message();
    EXPECT_EQ(import_response.shape_ids_size(), 1);
}

TEST_F(SimpleGrpcTest, InvalidBrepDataShouldFail) {
    geometry::BrepDataRequest request;
    request.set_brep_data("这不是有效的BREP数据");
    request.set_filename("invalid.brep");
    
    geometry::BrepImportResponse response;
    auto status = service_->LoadBrepFromData(&context_, &request, &response);
    
    // 应该返回错误状态或者success=false
    EXPECT_TRUE(!status.ok() || !response.success());
}

// ========== STEP File Tests ==========

// Test STEP export functionality
TEST_F(SimpleGrpcTest, StepExportShouldWork) {
    // Create a box first
    geometry::BoxRequest box_req;
    box_req.mutable_position()->set_x(0);
    box_req.mutable_position()->set_y(0);
    box_req.mutable_position()->set_z(0);
    box_req.set_width(4);
    box_req.set_height(4);
    box_req.set_depth(4);
    
    geometry::ShapeResponse box_resp;
    grpc::ServerContext ctx1;
    auto status = service_->CreateBox(&ctx1, &box_req, &box_resp);
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(box_resp.success());
    
    // Export to STEP
    geometry::StepExportRequest export_req;
    export_req.add_shape_ids(box_resp.shape_id());
    
    // Set export options
    auto* options = export_req.mutable_options();
    options->set_export_colors(true);
    options->set_export_names(true);
    options->set_schema_version("AP214");
    options->set_units("mm");
    
    geometry::StepFileResponse export_resp;
    grpc::ServerContext ctx2;
    status = service_->ExportStepFile(&ctx2, &export_req, &export_resp);
    
    ASSERT_TRUE(status.ok());
    EXPECT_TRUE(export_resp.success());
    EXPECT_FALSE(export_resp.step_data().empty());
    EXPECT_GT(export_resp.file_info().file_size(), 0);
    EXPECT_EQ(export_resp.file_info().shape_count(), 1);
    EXPECT_FALSE(export_resp.file_info().creation_time().empty());
    EXPECT_EQ(export_resp.file_info().schema_version(), "AP214");
}

// Test STEP export-import round trip
TEST_F(SimpleGrpcTest, StepExportImportRoundTrip) {
    // Create multiple shapes
    std::vector<std::string> original_shape_ids;
    
    // Create box
    geometry::BoxRequest box_req;
    box_req.mutable_position()->set_x(0);
    box_req.mutable_position()->set_y(0);
    box_req.mutable_position()->set_z(0);
    box_req.set_width(3);
    box_req.set_height(3);
    box_req.set_depth(3);
    
    geometry::ShapeResponse box_resp;
    grpc::ServerContext ctx1;
    service_->CreateBox(&ctx1, &box_req, &box_resp);
    original_shape_ids.push_back(box_resp.shape_id());
    
    // Create sphere  
    geometry::SphereRequest sphere_req;
    sphere_req.mutable_center()->set_x(5);
    sphere_req.mutable_center()->set_y(0);
    sphere_req.mutable_center()->set_z(0);
    sphere_req.set_radius(1.5);
    
    geometry::ShapeResponse sphere_resp;
    grpc::ServerContext ctx2;
    service_->CreateSphere(&ctx2, &sphere_req, &sphere_resp);
    original_shape_ids.push_back(sphere_resp.shape_id());
    
    // Export to STEP
    geometry::StepExportRequest export_req;
    for (const auto& id : original_shape_ids) {
        export_req.add_shape_ids(id);
    }
    
    geometry::StepFileResponse export_resp;
    grpc::ServerContext ctx3;
    service_->ExportStepFile(&ctx3, &export_req, &export_resp);
    
    ASSERT_TRUE(export_resp.success());
    std::string step_data = export_resp.step_data();
    EXPECT_FALSE(step_data.empty());
    
    // Clear all shapes
    geometry::EmptyRequest clear_req;
    geometry::StatusResponse clear_resp;
    grpc::ServerContext ctx4;
    service_->ClearAll(&ctx4, &clear_req, &clear_resp);
    
    // Import from STEP data
    geometry::StepDataRequest import_req;
    import_req.set_step_data(step_data);
    import_req.set_filename("round_trip_test.step");
    
    // Set import options
    auto* import_options = import_req.mutable_options();
    import_options->set_import_colors(true);
    import_options->set_import_names(true);
    
    geometry::StepImportResponse import_resp;
    grpc::ServerContext ctx5;
    auto status = service_->LoadStepFromData(&ctx5, &import_req, &import_resp);
    
    ASSERT_TRUE(status.ok());
    EXPECT_TRUE(import_resp.success());
    EXPECT_EQ(import_resp.shape_ids_size(), original_shape_ids.size());
    EXPECT_FALSE(import_resp.file_info().creation_time().empty());
}

// Test invalid STEP data
TEST_F(SimpleGrpcTest, InvalidStepDataShouldFail) {
    geometry::StepDataRequest request;
    request.set_step_data("This is definitely not valid STEP data!");
    request.set_filename("invalid.step");
    
    geometry::StepImportResponse response;
    grpc::ServerContext ctx;
    auto status = service_->LoadStepFromData(&ctx, &request, &response);
    
    ASSERT_TRUE(status.ok());  // gRPC call succeeds
    EXPECT_FALSE(response.success());  // But import fails
    EXPECT_FALSE(response.message().empty());  // Should have error message
}