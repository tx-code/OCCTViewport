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