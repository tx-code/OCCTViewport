#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>

#include "../../server/geometry_service_impl.h"

void RunServer(const std::string& server_address) {
    GeometryServiceImpl service;

    grpc::ServerBuilder builder;
    
    // Listen on the given address without any authentication mechanism
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to a *synchronous* service.
    builder.RegisterService(&service);
    
    // Finally assemble the server
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    
    spdlog::info("GeometryService server listening on {}", server_address);
    
    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

int main(int argc, char** argv) {
    // Set up logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    
    spdlog::info("Starting OCCT GeometryService gRPC Server...");
    
    // Default server address
    std::string server_address("0.0.0.0:50051");
    
    // Parse command line arguments
    if (argc > 1) {
        server_address = argv[1];
    }
    
    spdlog::info("Server address: {}", server_address);
    
    try {
        RunServer(server_address);
    } catch (const std::exception& e) {
        spdlog::error("Server error: {}", e.what());
        return 1;
    }
    
    return 0;
}