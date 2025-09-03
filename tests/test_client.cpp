#include "../src/client/grpc/geometry_client.h"
#include <iostream>
#include <spdlog/spdlog.h>

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    
    std::cout << "=== gRPC 网格数据传输测试 ===" << std::endl;
    
    // 创建客户端
    GeometryClient client("localhost:50051");
    
    // 连接到服务器
    if (!client.Connect()) {
        std::cout << "❌ 无法连接到服务器" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功连接到服务器" << std::endl;
    
    // 获取系统信息
    auto system_info = client.GetSystemInfo();
    std::cout << "服务器版本: " << system_info.version << std::endl;
    std::cout << "OCCT版本: " << system_info.occt_version << std::endl;
    std::cout << "活跃形状数: " << system_info.active_shapes << std::endl;
    
    // 创建演示场景
    std::cout << "\n=== 创建演示场景 ===" << std::endl;
    if (client.CreateDemoScene()) {
        std::cout << "✅ 演示场景创建成功" << std::endl;
    } else {
        std::cout << "❌ 演示场景创建失败" << std::endl;
        return 1;
    }
    
    // 获取所有网格
    std::cout << "\n=== 获取网格数据 ===" << std::endl;
    auto meshes = client.GetAllMeshes();
    
    std::cout << "接收到 " << meshes.size() << " 个网格:" << std::endl;
    
    for (size_t i = 0; i < meshes.size(); ++i) {
        const auto& mesh = meshes[i];
        std::cout << "网格 " << (i+1) << ":" << std::endl;
        std::cout << "  - 形状ID: " << mesh.shape_id << std::endl;
        std::cout << "  - 顶点数: " << mesh.vertices.size() / 3 << std::endl;
        std::cout << "  - 法线数: " << mesh.normals.size() / 3 << std::endl;
        std::cout << "  - 三角形数: " << mesh.indices.size() / 3 << std::endl;
        std::cout << "  - 颜色: (" << mesh.color[0] << ", " << mesh.color[1] << ", " << mesh.color[2] << ", " << mesh.color[3] << ")" << std::endl;
        
        if (!mesh.vertices.empty()) {
            std::cout << "  - 前3个顶点:" << std::endl;
            for (size_t j = 0; j < std::min(size_t(9), mesh.vertices.size()); j += 3) {
                std::cout << "    (" << mesh.vertices[j] << ", " << mesh.vertices[j+1] << ", " << mesh.vertices[j+2] << ")" << std::endl;
            }
        }
        std::cout << std::endl;
    }
    
    client.Disconnect();
    std::cout << "✅ 测试完成" << std::endl;
    return 0;
}