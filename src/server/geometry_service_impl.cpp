#include "geometry_service_impl.h"

// OCCT includes
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <Poly_Triangulation.hxx>
#include <AIS_DisplayMode.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <BRep_Tool.hxx>
#include <TopLoc_Location.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <Poly_Array1OfTriangle.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Poly_Triangle.hxx>

// STEP file includes
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <TDataStd_Name.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_LabelSequence.hxx>

// BREP file includes
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>

// Unified model import/export includes
#include <DE_Wrapper.hxx>
// Fallback includes for cases where DE_Wrapper is not available
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESControl_Writer.hxx>
#include <IGESControl_Controller.hxx>
#include <RWStl.hxx>
#include <RWObj_CafReader.hxx>
#include <RWObj_CafWriter.hxx>
#include <RWGltf_CafReader.hxx>
#include <RWGltf_CafWriter.hxx>
// File system operations
#include <filesystem>
#include <algorithm>
#include <cctype>

// Standard includes
#include <sstream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <spdlog/spdlog.h>

GeometryServiceImpl::GeometryServiceImpl() {
    spdlog::info("GeometryService: Initializing OCCT context...");
    
    // Initialize OCCT viewer (headless)
    Handle(Aspect_DisplayConnection) display_connection = new Aspect_DisplayConnection();
    Handle(OpenGl_GraphicDriver) graphic_driver = new OpenGl_GraphicDriver(display_connection, Standard_False);
    graphic_driver->ChangeOptions().buffersNoSwap = Standard_True;
    
    viewer_ = new V3d_Viewer(graphic_driver);
    viewer_->SetDefaultLights();
    viewer_->SetLightOn();
    
    context_ = new AIS_InteractiveContext(viewer_);
    context_->SetDisplayMode(AIS_Shaded, Standard_False);
    
    // Initialize session management
    last_cleanup_ = std::chrono::steady_clock::now();
    
    spdlog::info("GeometryService: OCCT context initialized successfully");
}

GeometryServiceImpl::~GeometryServiceImpl() {
    spdlog::info("GeometryService: Shutting down...");
}

std::string GeometryServiceImpl::generateShapeId() {
    // Deprecated - kept for backward compatibility
    // New code should use session->generateShapeId()
    return "shape_" + std::to_string(shape_counter_.fetch_add(1));
}

std::shared_ptr<GeometryServiceImpl::ClientSession> GeometryServiceImpl::getOrCreateSession(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // Clean up inactive sessions periodically (every 5 minutes)
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::minutes>(now - last_cleanup_).count() >= 5) {
        cleanupInactiveSessions();
        last_cleanup_ = now;
    }
    
    // Find or create session
    auto it = client_sessions_.find(client_id);
    if (it != client_sessions_.end()) {
        it->second->updateActivity();
        return it->second;
    }
    
    // Create new session
    auto session = std::make_shared<ClientSession>(client_id);
    client_sessions_[client_id] = session;
    spdlog::info("GeometryService: Created new session for client: {}", client_id);
    return session;
}

void GeometryServiceImpl::cleanupInactiveSessions(std::chrono::minutes timeout) {
    // Note: Must be called with sessions_mutex_ locked
    auto now = std::chrono::steady_clock::now();
    auto it = client_sessions_.begin();
    while (it != client_sessions_.end()) {
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - it->second->last_activity);
        if (duration >= timeout) {
            spdlog::info("GeometryService: Removing inactive session for client: {} (inactive for {} minutes)", 
                        it->first, duration.count());
            it = client_sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string GeometryServiceImpl::getClientId(grpc::ServerContext* context) const {
    if (!context) {
        spdlog::warn("getClientId: null context provided");
        return "Unknown";
    }
    
    auto metadata = context->client_metadata();
    auto it = metadata.find("client-id");
    if (it != metadata.end()) {
        return std::string(it->second.data(), it->second.length());
    }
    
    spdlog::warn("getClientId: no client-id metadata found");
    return "Unknown";
}

grpc::Status GeometryServiceImpl::CreateBox(grpc::ServerContext* context,
                                           const geometry::BoxRequest* request,
                                           geometry::ShapeResponse* response) {
    try {
        std::string client_id = getClientId(context);
        spdlog::info("[{}] CreateBox: width={}, height={}, depth={}", 
                    client_id, request->width(), request->height(), request->depth());
        
        // Get or create session for this client
        auto session = getOrCreateSession(client_id);
        
        std::string shape_id = session->generateShapeId();
        Handle(AIS_Shape) ais_shape = createBoxShape(*request);
        
        if (ais_shape.IsNull()) {
            response->set_success(false);
            response->set_message("Failed to create box shape");
            return grpc::Status::OK;
        }
        
        // Store shape data in session
        ShapeData shape_data;
        shape_data.ais_shape = ais_shape;
        shape_data.topo_shape = ais_shape->Shape();
        shape_data.color = request->color();
        shape_data.shape_id = shape_id;
        
        session->shapes[shape_id] = std::move(shape_data);
        
        // Set response
        response->set_shape_id(shape_id);
        response->set_success(true);
        response->set_message("Box created successfully");
        
        // Set shape properties in response
        auto* properties = response->mutable_properties();
        properties->set_shape_id(shape_id);
        properties->set_visible(true);
        // properties->set_selected(false); // Field removed in simplified proto
        // properties->set_highlighted(false); // Field removed in simplified proto
        properties->mutable_color()->CopyFrom(request->color());
        
        spdlog::info("[{}] CreateBox: Successfully created box with ID: {} (session has {} shapes)", 
                    client_id, shape_id, session->shapes.size());
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        spdlog::error("CreateBox: Exception occurred: {}", e.what());
        response->set_success(false);
        response->set_message("Internal server error: " + std::string(e.what()));
        return grpc::Status::OK;
    }
}

grpc::Status GeometryServiceImpl::CreateCone(grpc::ServerContext* context,
                                            const geometry::ConeRequest* request,
                                            geometry::ShapeResponse* response) {
    try {
        std::string client_id = getClientId(context);
        spdlog::info("[{}] CreateCone: base_radius={}, top_radius={}, height={}", client_id, 
                    request->base_radius(), request->top_radius(), request->height());
        
        // Get or create session for this client
        auto session = getOrCreateSession(client_id);
        
        std::string shape_id = session->generateShapeId();
        Handle(AIS_Shape) ais_shape = createConeShape(*request);
        
        if (ais_shape.IsNull()) {
            response->set_success(false);
            response->set_message("Failed to create cone shape");
            return grpc::Status::OK;
        }
        
        // Store shape data in session
        ShapeData shape_data;
        shape_data.ais_shape = ais_shape;
        shape_data.topo_shape = ais_shape->Shape();
        shape_data.color = request->color();
        shape_data.shape_id = shape_id;
        
        session->shapes[shape_id] = std::move(shape_data);
        
        // Set response
        response->set_shape_id(shape_id);
        response->set_success(true);
        response->set_message("Cone created successfully");
        
        // Set shape properties
        auto* properties = response->mutable_properties();
        properties->set_shape_id(shape_id);
        properties->set_visible(true);
        // properties->set_selected(false); // Field removed in simplified proto
        // properties->set_highlighted(false); // Field removed in simplified proto
        properties->mutable_color()->CopyFrom(request->color());
        
        spdlog::info("[{}] CreateCone: Successfully created cone with ID: {} (session has {} shapes)", 
                    client_id, shape_id, session->shapes.size());
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        spdlog::error("CreateCone: Exception occurred: {}", e.what());
        response->set_success(false);
        response->set_message("Internal server error: " + std::string(e.what()));
        return grpc::Status::OK;
    }
}

Handle(AIS_Shape) GeometryServiceImpl::createBoxShape(const geometry::BoxRequest& request) {
    gp_Pnt position = fromProtoPoint(request.position());
    gp_Ax2 axes(position, gp_Dir(0, 0, 1));
    
    TopoDS_Shape box = BRepPrimAPI_MakeBox(axes, request.width(), request.height(), request.depth()).Shape();
    Handle(AIS_Shape) ais_shape = new AIS_Shape(box);
    
    // Set color
    Quantity_Color occt_color = fromProtoColor(request.color());
    ais_shape->SetColor(occt_color);
    
    return ais_shape;
}

Handle(AIS_Shape) GeometryServiceImpl::createConeShape(const geometry::ConeRequest& request) {
    gp_Pnt position = fromProtoPoint(request.position());
    gp_Vec axis_vec = fromProtoVector(request.axis());
    gp_Ax2 axes(position, gp_Dir(axis_vec));
    
    TopoDS_Shape cone = BRepPrimAPI_MakeCone(axes, request.base_radius(), 
                                            request.top_radius(), request.height()).Shape();
    Handle(AIS_Shape) ais_shape = new AIS_Shape(cone);
    
    // Set color
    Quantity_Color occt_color = fromProtoColor(request.color());
    ais_shape->SetColor(occt_color);
    
    return ais_shape;
}

// Conversion helper methods
geometry::Point3D GeometryServiceImpl::toProtoPoint(const gp_Pnt& point) {
    geometry::Point3D proto_point;
    proto_point.set_x(point.X());
    proto_point.set_y(point.Y());
    proto_point.set_z(point.Z());
    return proto_point;
}

geometry::Vector3D GeometryServiceImpl::toProtoVector(const gp_Vec& vector) {
    geometry::Vector3D proto_vector;
    proto_vector.set_x(vector.X());
    proto_vector.set_y(vector.Y());
    proto_vector.set_z(vector.Z());
    return proto_vector;
}

geometry::Color GeometryServiceImpl::toProtoColor(const Quantity_Color& color) {
    geometry::Color proto_color;
    proto_color.set_r(color.Red());
    proto_color.set_g(color.Green());
    proto_color.set_b(color.Blue());
    proto_color.set_a(1.0); // OCCT doesn't have alpha, default to opaque
    return proto_color;
}

gp_Pnt GeometryServiceImpl::fromProtoPoint(const geometry::Point3D& point) {
    return gp_Pnt(point.x(), point.y(), point.z());
}

gp_Vec GeometryServiceImpl::fromProtoVector(const geometry::Vector3D& vector) {
    return gp_Vec(vector.x(), vector.y(), vector.z());
}

Quantity_Color GeometryServiceImpl::fromProtoColor(const geometry::Color& color) {
    return Quantity_Color(color.r(), color.g(), color.b(), Quantity_TOC_RGB);
}

grpc::Status GeometryServiceImpl::GetSystemInfo(grpc::ServerContext* context,
                                               const geometry::EmptyRequest* request,
                                               geometry::SystemInfoResponse* response) {
    std::string client_id = getClientId(context);
    auto session = getOrCreateSession(client_id);
    
    response->set_version("1.0.0");
    response->set_active_shapes(static_cast<int32_t>(session->shapes.size()));
    
    // Use OCCT version from CMake compile definition
#ifdef OCCT_VERSION
    response->set_occt_version(OCCT_VERSION);
#else
    response->set_occt_version("Unknown");
#endif
    
    // Add session info to log - capture session shape count safely
    size_t total_sessions = 0;
    size_t session_shape_count = session->shapes.size();
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        total_sessions = client_sessions_.size();
    }
    
    spdlog::info("[{}] GetSystemInfo: Session has {} shapes, total {} active sessions", 
                client_id, session_shape_count, total_sessions);
    return grpc::Status::OK;
}


// Placeholder implementations for remaining methods
grpc::Status GeometryServiceImpl::CreateSphere(grpc::ServerContext* context,
                                              const geometry::SphereRequest* request,
                                              geometry::ShapeResponse* response) {
    try {
        std::string client_id = getClientId(context);
        spdlog::info("[{}] CreateSphere: radius={}", client_id, request->radius());
        
        // Get or create session for this client
        auto session = getOrCreateSession(client_id);
        
        std::string shape_id = session->generateShapeId();
        Handle(AIS_Shape) ais_shape = createSphereShape(*request);
        
        if (ais_shape.IsNull()) {
            response->set_success(false);
            response->set_message("Failed to create sphere shape");
            return grpc::Status::OK;
        }
        
        // Store shape data in session
        ShapeData shape_data;
        shape_data.ais_shape = ais_shape;
        shape_data.topo_shape = ais_shape->Shape();
        shape_data.color = request->color();
        shape_data.shape_id = shape_id;
        
        session->shapes[shape_id] = std::move(shape_data);
        
        // Build response
        response->set_success(true);
        response->set_shape_id(shape_id);
        response->set_message("Sphere created successfully");
        
        spdlog::info("[{}] CreateSphere: Successfully created sphere with ID: {} (session has {} shapes)", 
                    client_id, shape_id, session->shapes.size());
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        spdlog::error("CreateSphere: Exception occurred: {}", e.what());
        response->set_success(false);
        response->set_message("Internal server error: " + std::string(e.what()));
        return grpc::Status::OK;
    }
}

grpc::Status GeometryServiceImpl::CreateCylinder(grpc::ServerContext* context,
                                                const geometry::CylinderRequest* request,
                                                geometry::ShapeResponse* response) {
    try {
        std::string client_id = getClientId(context);
        spdlog::info("[{}] CreateCylinder: radius={}, height={}", client_id, request->radius(), request->height());
        
        // Get or create session for this client
        auto session = getOrCreateSession(client_id);
        
        std::string shape_id = session->generateShapeId();
        Handle(AIS_Shape) ais_shape = createCylinderShape(*request);
        
        if (ais_shape.IsNull()) {
            response->set_success(false);
            response->set_message("Failed to create cylinder shape");
            return grpc::Status::OK;
        }
        
        // Store shape data in session
        ShapeData shape_data;
        shape_data.ais_shape = ais_shape;
        shape_data.topo_shape = ais_shape->Shape();
        shape_data.color = request->color();
        shape_data.shape_id = shape_id;
        
        session->shapes[shape_id] = std::move(shape_data);
        
        // Build response
        response->set_success(true);
        response->set_shape_id(shape_id);
        response->set_message("Cylinder created successfully");
        
        spdlog::info("[{}] CreateCylinder: Successfully created cylinder with ID: {} (session has {} shapes)", 
                    client_id, shape_id, session->shapes.size());
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        spdlog::error("CreateCylinder: Exception occurred: {}", e.what());
        response->set_success(false);
        response->set_message("Internal server error: " + std::string(e.what()));
        return grpc::Status::OK;
    }
}

grpc::Status GeometryServiceImpl::DeleteShape(grpc::ServerContext* context,
                                             const geometry::ShapeRequest* request,
                                             geometry::StatusResponse* response) {
    std::string client_id = getClientId(context);
    auto session = getOrCreateSession(client_id);
    
    std::string shape_id = request->shape_id();
    auto it = session->shapes.find(shape_id);
    if (it == session->shapes.end()) {
        response->set_success(false);
        response->set_message("Shape not found in your session: " + shape_id);
        return grpc::Status::OK;
    }
    
    session->shapes.erase(it);
    response->set_success(true);
    response->set_message("Shape deleted successfully: " + shape_id);
    
    spdlog::info("[{}] DeleteShape: Deleted shape {} (session now has {} shapes)", 
                client_id, shape_id, session->shapes.size());
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::TransformShape(grpc::ServerContext* context,
                                                const geometry::TransformRequest* request,
                                                geometry::ShapeResponse* response) {
    response->set_success(false);
    response->set_message("TransformShape not implemented yet");
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::SetShapeColor(grpc::ServerContext* context,
                                               const geometry::ColorRequest* request,
                                               geometry::StatusResponse* response) {
    response->set_success(false);
    response->set_message("SetShapeColor not implemented yet");
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::GetMeshData(grpc::ServerContext* context,
                                             const geometry::ShapeRequest* request,
                                             geometry::MeshData* response) {
    if (!connected_) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Service not connected");
    }
    
    std::string client_id = getClientId(context);
    auto session = getOrCreateSession(client_id);
    
    std::string shape_id = request->shape_id();
    auto it = session->shapes.find(shape_id);
    if (it == session->shapes.end()) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Shape not found in your session: " + shape_id);
    }
    
    try {
        spdlog::info("[{}] GetMeshData: Extracting mesh for shape: {}", client_id, shape_id);
        *response = extractMeshData(shape_id);
        spdlog::info("[{}] GetMeshData: Successfully extracted mesh with {} vertices, {} indices", client_id, 
                    response->vertices_size(), response->indices_size());
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        spdlog::error("GetMeshData: Exception occurred: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to extract mesh data: " + std::string(e.what()));
    }
}

grpc::Status GeometryServiceImpl::GetAllMeshes(grpc::ServerContext* context,
                                              const geometry::EmptyRequest* request,
                                              grpc::ServerWriter<geometry::MeshData>* writer) {
    try {
        std::string client_id = getClientId(context);
        auto session = getOrCreateSession(client_id);
        
        spdlog::info("[{}] GetAllMeshes: Streaming {} shapes from client session", client_id, session->shapes.size());
        
        for (const auto& [shape_id, shape_data] : session->shapes) {
            geometry::MeshData mesh_data = extractMeshData(shape_id);
            if (!writer->Write(mesh_data)) {
                spdlog::error("GetAllMeshes: Failed to write mesh data for shape: {}", shape_id);
                break;
            }
            spdlog::info("[{}] GetAllMeshes: Sent mesh for shape: {} ({} vertices)", client_id, 
                        shape_id, mesh_data.vertices_size());
        }
        
        spdlog::info("[{}] GetAllMeshes: Completed streaming all meshes for session", client_id);
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        spdlog::error("GetAllMeshes: Exception occurred: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to stream mesh data: " + std::string(e.what()));
    }
}

grpc::Status GeometryServiceImpl::ClearAll(grpc::ServerContext* context,
                                          const geometry::EmptyRequest* request,
                                          geometry::StatusResponse* response) {
    std::string client_id = getClientId(context);
    
    // Get session for this client
    auto session = getOrCreateSession(client_id);
    
    // Clear only this client's shapes
    size_t shapes_cleared = session->shapes.size();
    session->shapes.clear();
    
    response->set_success(true);
    response->set_message("Cleared " + std::to_string(shapes_cleared) + " shapes for this session");
    spdlog::info("[{}] ClearAll: Cleared {} shapes for this client session", client_id, shapes_cleared);
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::CreateDemoScene(grpc::ServerContext* context,
                                                 const geometry::EmptyRequest* request,
                                                 geometry::StatusResponse* response) {
    try {
        std::string client_id = getClientId(context);
        spdlog::info("[{}] CreateDemoScene: Creating demo geometry objects...", client_id);
        
        int created_count = 0;
        
        // Create Box at (0, 0, 0) with size 50x50x50
        {
            geometry::BoxRequest box_request;
            auto* position = box_request.mutable_position();
            position->set_x(0.0);
            position->set_y(0.0);
            position->set_z(0.0);
            
            box_request.set_width(50.0);
            box_request.set_height(50.0);
            box_request.set_depth(50.0);
            
            auto* color = box_request.mutable_color();
            color->set_r(0.8);
            color->set_g(0.8);
            color->set_b(0.8);
            color->set_a(1.0);
            
            geometry::ShapeResponse box_response;
            grpc::Status status = CreateBox(context, &box_request, &box_response);
            if (status.ok() && box_response.success()) {
                created_count++;
                spdlog::info("[{}] CreateDemoScene: Created demo box with ID: {}", client_id, box_response.shape_id());
            }
        }
        
        // Create Cone at (25, 125, 0) with base_radius=25, top_radius=0, height=50
        {
            geometry::ConeRequest cone_request;
            auto* position = cone_request.mutable_position();
            position->set_x(25.0);
            position->set_y(125.0);
            position->set_z(0.0);
            
            auto* axis = cone_request.mutable_axis();
            axis->set_x(0.0);
            axis->set_y(0.0);
            axis->set_z(1.0);
            
            cone_request.set_base_radius(25.0);
            cone_request.set_top_radius(0.0);
            cone_request.set_height(50.0);
            
            auto* color = cone_request.mutable_color();
            color->set_r(0.7);
            color->set_g(0.9);
            color->set_b(0.7);
            color->set_a(1.0);
            
            geometry::ShapeResponse cone_response;
            grpc::Status status = CreateCone(context, &cone_request, &cone_response);
            if (status.ok() && cone_response.success()) {
                created_count++;
                spdlog::info("[{}] CreateDemoScene: Created demo cone with ID: {}", client_id, cone_response.shape_id());
            }
        }
        
        response->set_success(true);
        response->set_message("Demo scene created successfully. Created " + std::to_string(created_count) + " objects.");
        
        spdlog::info("[{}] CreateDemoScene: Successfully created demo scene with {} objects", client_id, created_count);
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        spdlog::error("CreateDemoScene: Exception occurred: {}", e.what());
        response->set_success(false);
        response->set_message("Failed to create demo scene: " + std::string(e.what()));
        return grpc::Status::OK;
    }
}

geometry::MeshData GeometryServiceImpl::extractMeshData(const std::string& shape_id) {
    geometry::MeshData mesh_data;
    mesh_data.set_shape_id(shape_id);
    // mesh_data.set_version(1); // Field removed in simplified proto
    
    // Thread-safe shape data lookup with copy
    ShapeData shape_data_copy;
    bool shape_found = false;
    
    // First check deprecated global shapes_
    auto it = shapes_.find(shape_id);
    if (it != shapes_.end()) {
        shape_data_copy = it->second;  // Make a copy
        shape_found = true;
    } else {
        // Search through all sessions with proper locking
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (const auto& [client_id, session] : client_sessions_) {
            auto session_it = session->shapes.find(shape_id);
            if (session_it != session->shapes.end()) {
                shape_data_copy = session_it->second;  // Make a copy while holding lock
                shape_found = true;
                break;
            }
        }
        // Lock automatically released here, but we have our own copy
    }
    
    if (!shape_found) {
        spdlog::error("extractMeshData: Shape not found: {}", shape_id);
        return mesh_data;
    }
    
    // Now work with our thread-safe copy
    const TopoDS_Shape& shape = shape_data_copy.topo_shape;
    
    // Generate mesh if not already done
    BRepMesh_IncrementalMesh mesh(shape, 0.1, Standard_False, 0.5, Standard_True);
    if (!mesh.IsDone()) {
        spdlog::error("extractMeshData: Failed to generate mesh for shape: {}", shape_id);
        return mesh_data;
    }
    
    std::vector<gp_Pnt> vertices;
    std::vector<gp_Vec> normals;
    std::vector<int> indices;
    
    // Extract triangulation from all faces
    TopExp_Explorer face_explorer(shape, TopAbs_FACE);
    int vertex_offset = 0;
    
    for (; face_explorer.More(); face_explorer.Next()) {
        const TopoDS_Face& face = TopoDS::Face(face_explorer.Current());
        TopLoc_Location location;
        
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) {
            continue;
        }
        
        // Get node and triangle data using new API
        int nb_nodes = triangulation->NbNodes();
        int nb_triangles = triangulation->NbTriangles();
        
        // Transform vertices by location
        gp_Trsf transform = location.Transformation();
        
        // Add vertices
        int face_vertex_start = vertices.size();
        for (int i = 1; i <= nb_nodes; i++) {
            gp_Pnt point = triangulation->Node(i);
            point.Transform(transform);
            vertices.push_back(point);
        }
        
        // Add triangles (convert to indices)
        Standard_Boolean reverse_orientation = (face.Orientation() == TopAbs_REVERSED);
        
        for (int i = 1; i <= nb_triangles; i++) {
            const Poly_Triangle& triangle = triangulation->Triangle(i);
            Standard_Integer n1, n2, n3;
            triangle.Get(n1, n2, n3);
            
            // Convert to 0-based indexing and add vertex offset
            int idx1 = face_vertex_start + (n1 - 1);
            int idx2 = face_vertex_start + (n2 - 1);
            int idx3 = face_vertex_start + (n3 - 1);
            
            if (reverse_orientation) {
                indices.push_back(idx1);
                indices.push_back(idx3);
                indices.push_back(idx2);
            } else {
                indices.push_back(idx1);
                indices.push_back(idx2);
                indices.push_back(idx3);
            }
        }
        
        // Calculate normals for this face (simplified approach)
        gp_Vec face_normal(0, 0, 1); // Placeholder - should calculate actual normal
        for (int i = face_vertex_start; i < vertices.size(); i++) {
            normals.push_back(face_normal);
        }
    }
    
    // Convert to protobuf format
    for (const gp_Pnt& vertex : vertices) {
        geometry::Point3D* proto_vertex = mesh_data.add_vertices();
        proto_vertex->set_x(vertex.X());
        proto_vertex->set_y(vertex.Y());
        proto_vertex->set_z(vertex.Z());
    }
    
    for (const gp_Vec& normal : normals) {
        geometry::Vector3D* proto_normal = mesh_data.add_normals();
        proto_normal->set_x(normal.X());
        proto_normal->set_y(normal.Y());
        proto_normal->set_z(normal.Z());
    }
    
    for (int index : indices) {
        mesh_data.add_indices(index);
    }
    
    // Set color from our thread-safe copy
    mesh_data.mutable_color()->CopyFrom(shape_data_copy.color);
    
    // Calculate bounding box (simplified)
    if (!vertices.empty()) {
        gp_Pnt min_pt = vertices[0];
        gp_Pnt max_pt = vertices[0];
        
        for (const gp_Pnt& vertex : vertices) {
            if (vertex.X() < min_pt.X()) min_pt.SetX(vertex.X());
            if (vertex.Y() < min_pt.Y()) min_pt.SetY(vertex.Y());
            if (vertex.Z() < min_pt.Z()) min_pt.SetZ(vertex.Z());
            if (vertex.X() > max_pt.X()) max_pt.SetX(vertex.X());
            if (vertex.Y() > max_pt.Y()) max_pt.SetY(vertex.Y());
            if (vertex.Z() > max_pt.Z()) max_pt.SetZ(vertex.Z());
        }
        
        auto* bounding_box = mesh_data.mutable_bounding_box();
        bounding_box->mutable_min()->CopyFrom(toProtoPoint(min_pt));
        bounding_box->mutable_max()->CopyFrom(toProtoPoint(max_pt));
    }
    
    spdlog::info("extractMeshData: Generated mesh for {}: {} vertices, {} triangles", 
                shape_id, vertices.size(), indices.size() / 3);
    
    return mesh_data;
}


Handle(AIS_Shape) GeometryServiceImpl::createSphereShape(const geometry::SphereRequest& request) {
    gp_Pnt center = fromProtoPoint(request.center());
    
    TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(center, request.radius()).Shape();
    Handle(AIS_Shape) ais_shape = new AIS_Shape(sphere);
    
    // Set color
    Quantity_Color occt_color = fromProtoColor(request.color());
    ais_shape->SetColor(occt_color);
    
    return ais_shape;
}

Handle(AIS_Shape) GeometryServiceImpl::createCylinderShape(const geometry::CylinderRequest& request) {
    gp_Pnt position = fromProtoPoint(request.position());
    gp_Ax2 axes(position, gp::DZ());  // Default axis along Z-direction
    
    TopoDS_Shape cylinder = BRepPrimAPI_MakeCylinder(axes, request.radius(), request.height()).Shape();
    Handle(AIS_Shape) ais_shape = new AIS_Shape(cylinder);
    
    // Set color
    Quantity_Color occt_color = fromProtoColor(request.color());
    ais_shape->SetColor(occt_color);
    
    return ais_shape;
}

// =============================================================================
// Unified Model Import/Export Implementation
// =============================================================================

grpc::Status GeometryServiceImpl::ImportModelFile(grpc::ServerContext* context,
                                                 const geometry::ModelFileRequest* request,
                                                 geometry::ModelImportResponse* response) {
    std::string client_id = getClientId(context);
    spdlog::info("[{}] ImportModelFile: Importing file: {}", client_id, request->file_path());
    
    // Get or create session for this client
    auto session = getOrCreateSession(client_id);
    
    try {
        // Import with proper exception safety
        std::vector<std::string> shape_ids = importModelFileInternal(
            request->file_path(), request->options());
        
        // Move shapes from global to session with rollback capability
        std::vector<std::string> session_shape_ids;
        std::vector<std::pair<std::string, ShapeData>> backup_shapes;
        
        try {
            for (const auto& shape_id : shape_ids) {
                auto it = shapes_.find(shape_id);
                if (it != shapes_.end()) {
                    // Generate new session-specific ID
                    std::string new_shape_id = session->generateShapeId();
                    
                    // Back up the original shape for rollback
                    backup_shapes.emplace_back(shape_id, it->second);
                    
                    // Move to session
                    ShapeData shape_data = std::move(it->second);
                    shape_data.shape_id = new_shape_id;
                    session->shapes[new_shape_id] = std::move(shape_data);
                    shapes_.erase(it);
                    
                    session_shape_ids.push_back(new_shape_id);
                }
            }
            shape_ids = std::move(session_shape_ids);  // Use session IDs
        } catch (...) {
            // Rollback: restore backed up shapes
            for (const auto& [orig_id, shape_data] : backup_shapes) {
                shapes_[orig_id] = shape_data;
            }
            // Remove any partially added shapes from session
            for (const auto& session_id : session_shape_ids) {
                session->shapes.erase(session_id);
            }
            throw;  // Re-throw original exception
        }
        
        response->set_success(true);
        response->set_message("Model file imported successfully");
        
        for (const auto& shape_id : shape_ids) {
            response->add_shape_ids(shape_id);
        }
        
        // Detect format and set file info
        std::string detected_format = detectFileFormat(request->file_path(), request->options().force_format());
        response->set_detected_format(detected_format);
        
        geometry::ModelFileInfo file_info = getModelFileInfo(
            request->file_path(), "", shape_ids.size(), detected_format);
        response->mutable_file_info()->CopyFrom(file_info);
        
        spdlog::info("[{}] ImportModelFile: Successfully imported {} shapes from {} (format: {}, session has {} total shapes)", 
                    client_id, shape_ids.size(), request->file_path(), detected_format, session->shapes.size());
        
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("Failed to import model file: ") + e.what());
        spdlog::error("ImportModelFile: Exception: {}", e.what());
    }
    
    return grpc::Status::OK;
}


grpc::Status GeometryServiceImpl::ExportModelFile(grpc::ServerContext* context,
                                                 const geometry::ModelExportRequest* request,
                                                 geometry::ModelFileResponse* response) {
    std::string client_id = getClientId(context);
    auto session = getOrCreateSession(client_id);
    
    spdlog::info("[{}] ExportModelFile: Exporting {} shapes to format: {}", client_id, 
                request->shape_ids_size(), request->options().format());
    
    try {
        std::vector<std::string> shape_ids(request->shape_ids().begin(), request->shape_ids().end());
        std::string model_data;
        geometry::ModelFileInfo file_info;
        
        // Temporarily copy session shapes to global shapes_ for export
        std::vector<std::string> temp_added;
        for (const auto& shape_id : shape_ids) {
            auto it = session->shapes.find(shape_id);
            if (it != session->shapes.end() && shapes_.find(shape_id) == shapes_.end()) {
                shapes_[shape_id] = it->second;
                temp_added.push_back(shape_id);
            }
        }
        
        bool success = exportModelFileInternal(shape_ids, request->options(), model_data, file_info);
        
        // Clean up temporary shapes
        for (const auto& shape_id : temp_added) {
            shapes_.erase(shape_id);
        }
        
        if (success) {
            response->set_success(true);
            response->set_message("Model file exported successfully");
            response->set_model_data(model_data);
            response->set_filename(file_info.filename());
            response->mutable_file_info()->CopyFrom(file_info);
            
            spdlog::info("[{}] ExportModelFile: Successfully exported {} shapes to {} format, size: {} bytes", client_id, 
                        shape_ids.size(), request->options().format(), model_data.size());
        } else {
            response->set_success(false);
            response->set_message("Failed to export model file");
        }
        
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("Failed to export model file: ") + e.what());
        spdlog::error("ExportModelFile: Exception: {}", e.what());
    }
    
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::DisconnectClient(grpc::ServerContext* context,
                                                  const geometry::EmptyRequest* request,
                                                  geometry::StatusResponse* response) {
    std::string client_id = getClientId(context);
    
    // Remove client session if it exists
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = client_sessions_.find(client_id);
        if (it != client_sessions_.end()) {
            size_t shape_count = it->second->shapes.size();
            client_sessions_.erase(it);
            spdlog::info("[{}] DisconnectClient: Session removed, cleared {} shapes, {} active sessions remaining", 
                        client_id, shape_count, client_sessions_.size());
        } else {
            spdlog::warn("[{}] DisconnectClient: No active session found for client", client_id);
        }
    }
    
    response->set_success(true);
    response->set_message("Client disconnected successfully");
    return grpc::Status::OK;
}

// This file contains the unified model import/export helper method implementations
// It will be appended to geometry_service_impl.cpp

// =============================================================================
// Unified Model Import/Export Helper Methods Implementation
// =============================================================================

std::vector<std::string> GeometryServiceImpl::importModelFileInternal(
    const std::string& file_path, const geometry::ModelImportOptions& options) {
    
    std::vector<std::string> shape_ids;
    
    // Detect file format
    std::string format = detectFileFormat(file_path, options.force_format());
    spdlog::info("importModelFileInternal: Detected format '{}' for file: {}", format, file_path);
    
    // First try using DE_Wrapper for unified import
    try {
        #ifdef HAVE_DE_WRAPPER
        Handle(DE_Wrapper) wrapper = DE_Wrapper::GlobalWrapper();
        if (!wrapper.IsNull()) {
            TopoDS_Shape shape;
            TCollection_AsciiString path(file_path.c_str());
            
            if (wrapper->Read(path, shape)) {
                if (!shape.IsNull()) {
                    std::string shape_id = generateShapeId();
                    Handle(AIS_Shape) ais_shape = new AIS_Shape(shape);
                    
                    // Store shape data
                    ShapeData shape_data;
                    shape_data.ais_shape = ais_shape;
                    shape_data.topo_shape = shape;
                    shape_data.color.set_r(0.8);
                    shape_data.color.set_g(0.8);
                    shape_data.color.set_b(0.8);
                    shape_data.color.set_a(1.0); // Default color
                    shape_data.shape_id = shape_id;
                    
                    shapes_[shape_id] = std::move(shape_data);
                    shape_ids.push_back(shape_id);
                    
                    spdlog::info("importModelFileInternal: Successfully imported using DE_Wrapper: {}", shape_id);
                    return shape_ids;
                }
            }
        }
        #endif
    } catch (const std::exception& e) {
        spdlog::warn("importModelFileInternal: DE_Wrapper failed: {}", e.what());
    }
    
    // Fallback to format-specific readers
    return importModelFileInternal_FormatSpecific(file_path, format, options);
}

std::vector<std::string> GeometryServiceImpl::importModelFileInternal_FormatSpecific(
    const std::string& file_path, const std::string& format, const geometry::ModelImportOptions& options) {
    
    std::vector<std::string> shape_ids;
    
    try {
        if (format == "STEP" || format == "STP") {
            // Use STEPControl_Reader for STEP import
            STEPControl_Reader reader;
            IFSelect_ReturnStatus status = reader.ReadFile(file_path.c_str());
            
            if (status != IFSelect_RetDone) {
                throw std::runtime_error("Failed to read STEP file");
            }
            
            // Transfer roots
            Standard_Integer nb_roots = reader.NbRootsForTransfer();
            reader.TransferRoots();
            
            // Get the shape
            TopoDS_Shape shape = reader.OneShape();
            if (shape.IsNull()) {
                throw std::runtime_error("Failed to transfer STEP data");
            }
            
            // Store the shape
            std::string shape_id = generateShapeId();
            
            Handle(AIS_Shape) ais_shape = new AIS_Shape(shape);
            ais_shape->SetDisplayMode(AIS_Shaded);
            
            ShapeData shape_data;
            shape_data.ais_shape = ais_shape;
            shape_data.topo_shape = shape;
            shape_data.shape_id = shape_id;
            shape_data.visible = true;
            shape_data.color.set_r(0.7f);
            shape_data.color.set_g(0.7f);
            shape_data.color.set_b(0.7f);
            shape_data.color.set_a(1.0f);
            
            shapes_[shape_id] = std::move(shape_data);
            shape_ids.push_back(shape_id);
            
            spdlog::info("Successfully imported STEP file: {} shape(s)", shape_ids.size());
            return shape_ids;
            
        } else if (format == "IGES" || format == "IGS") {
            return importIgesFileInternal(file_path, options);
            
        } else if (format == "BREP" || format == "BRP") {
            // Use BRepTools for BREP import
            TopoDS_Shape shape;
            BRep_Builder builder;
            
            if (!BRepTools::Read(shape, file_path.c_str(), builder)) {
                throw std::runtime_error("Failed to read BREP file");
            }
            
            if (shape.IsNull()) {
                throw std::runtime_error("Invalid BREP data");
            }
            
            // Store the shape
            std::string shape_id = generateShapeId();
            
            Handle(AIS_Shape) ais_shape = new AIS_Shape(shape);
            ais_shape->SetDisplayMode(AIS_Shaded);
            
            ShapeData shape_data;
            shape_data.ais_shape = ais_shape;
            shape_data.topo_shape = shape;
            shape_data.shape_id = shape_id;
            shape_data.visible = true;
            shape_data.color.set_r(0.7f);
            shape_data.color.set_g(0.7f);
            shape_data.color.set_b(0.7f);
            shape_data.color.set_a(1.0f);
            
            shapes_[shape_id] = std::move(shape_data);
            shape_ids.push_back(shape_id);
            
            spdlog::info("Successfully imported BREP file: {} shape(s)", shape_ids.size());
            return shape_ids;
            
        } else if (format == "STL") {
            return importStlFileInternal(file_path, options);
            
        } else if (format == "OBJ") {
            return importObjFileInternal(file_path, options);
            
        } else if (format == "PLY") {
            return importPlyFileInternal(file_path, options);
            
        } else if (format == "GLTF" || format == "GLB") {
            return importGltfFileInternal(file_path, options);
            
        } else {
            throw std::runtime_error("Unsupported file format: " + format);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("importModelFileInternal_FormatSpecific: Failed to import {} file: {}", format, e.what());
        throw;
    }
}

std::vector<std::string> GeometryServiceImpl::importStlFileInternal(
    const std::string& file_path, const geometry::ModelImportOptions& options) {
    
    std::vector<std::string> shape_ids;
    
    try {
        // Use RWStl to read STL file - returns a triangulation
        Handle(Poly_Triangulation) triangulation = RWStl::ReadFile(file_path.c_str());
        if (triangulation.IsNull()) {
            throw std::runtime_error("Failed to read STL file using RWStl");
        }
        
        // Convert triangulation to shape (create a face from the triangulation)
        TopoDS_Face face;
        BRep_Builder builder;
        builder.MakeFace(face);
        builder.UpdateFace(face, triangulation);
        TopoDS_Shape shape = face;
        
        if (shape.IsNull()) {
            throw std::runtime_error("STL file produced null shape");
        }
        
        // Create AIS shape
        std::string shape_id = generateShapeId();
        Handle(AIS_Shape) ais_shape = new AIS_Shape(shape);
        
        // Set default color for STL (no color info in STL files)
        Quantity_Color default_color(0.7, 0.7, 0.9, Quantity_TOC_RGB);
        ais_shape->SetColor(default_color);
        
        // Store shape data
        ShapeData shape_data;
        shape_data.ais_shape = ais_shape;
        shape_data.topo_shape = shape;
        shape_data.color.set_r(0.7);
        shape_data.color.set_g(0.7);
        shape_data.color.set_b(0.9);
        shape_data.color.set_a(1.0);
        shape_data.shape_id = shape_id;
        
        shapes_[shape_id] = std::move(shape_data);
        shape_ids.push_back(shape_id);
        
        spdlog::info("importStlFileInternal: Successfully imported STL file: {}", shape_id);
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to import STL file '" + file_path + "': " + e.what());
    }
    
    return shape_ids;
}

std::vector<std::string> GeometryServiceImpl::importIgesFileInternal(
    const std::string& file_path, const geometry::ModelImportOptions& options) {
    
    std::vector<std::string> shape_ids;
    
    try {
        IGESControl_Reader reader;
        
        // Read the IGES file
        IFSelect_ReturnStatus status = reader.ReadFile(file_path.c_str());
        if (status != IFSelect_RetDone) {
            throw std::runtime_error("Failed to read IGES file");
        }
        
        // Transfer shapes
        reader.TransferRoots();
        int nb_shapes = reader.NbShapes();
        
        if (nb_shapes == 0) {
            throw std::runtime_error("No shapes found in IGES file");
        }
        
        for (int i = 1; i <= nb_shapes; i++) {
            TopoDS_Shape shape = reader.Shape(i);
            if (!shape.IsNull()) {
                std::string shape_id = generateShapeId();
                Handle(AIS_Shape) ais_shape = new AIS_Shape(shape);
                
                // Set default color
                Quantity_Color default_color(0.8, 0.7, 0.6, Quantity_TOC_RGB);
                ais_shape->SetColor(default_color);
                
                // Store shape data
                ShapeData shape_data;
                shape_data.ais_shape = ais_shape;
                shape_data.topo_shape = shape;
                shape_data.color.set_r(0.8);
                shape_data.color.set_g(0.7);
                shape_data.color.set_b(0.6);
                shape_data.color.set_a(1.0);
                shape_data.shape_id = shape_id;
                
                shapes_[shape_id] = std::move(shape_data);
                shape_ids.push_back(shape_id);
                
                spdlog::info("importIgesFileInternal: Successfully imported IGES shape: {}", shape_id);
            }
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to import IGES file '" + file_path + "': " + e.what());
    }
    
    return shape_ids;
}

std::vector<std::string> GeometryServiceImpl::importObjFileInternal(
    const std::string& file_path, const geometry::ModelImportOptions& options) {
    
    std::vector<std::string> shape_ids;
    
    try {
        // Create XDE document for OBJ import with materials/colors
        Handle(TDocStd_Document) doc;
        Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
        app->NewDocument("MDTV-XCAF", doc);
        
        RWObj_CafReader reader;
        
        // Set OBJ reader parameters
        reader.SetSinglePrecision(false);
        // Note: SetCreateShapes is not available in OCCT 7.9, shapes are created by default
        
        // Read OBJ file
        TCollection_AsciiString file_path_ascii(file_path.c_str());
        // For OCCT 7.9, use the new API
        reader.SetDocument(doc);
        if (!reader.Perform(file_path_ascii, Message_ProgressRange())) {
            throw std::runtime_error("Failed to read OBJ file");
        }
        
        // Extract shapes from document
        Handle(XCAFDoc_ShapeTool) shape_tool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
        TDF_LabelSequence free_labels;
        shape_tool->GetFreeShapes(free_labels);
        
        if (free_labels.Length() == 0) {
            throw std::runtime_error("No shapes found in OBJ file");
        }
        
        for (int i = 1; i <= free_labels.Length(); i++) {
            const TDF_Label& label = free_labels.Value(i);
            TopoDS_Shape shape;
            
            if (shape_tool->GetShape(label, shape) && !shape.IsNull()) {
                std::string shape_id = generateShapeId();
                Handle(AIS_Shape) ais_shape = new AIS_Shape(shape);
                
                // Try to get color from XDE
                Handle(XCAFDoc_ColorTool) color_tool = XCAFDoc_DocumentTool::ColorTool(doc->Main());
                Quantity_Color obj_color(0.6, 0.8, 0.7, Quantity_TOC_RGB); // Default OBJ color
                
                if (color_tool->GetColor(label, XCAFDoc_ColorSurf, obj_color)) {
                    ais_shape->SetColor(obj_color);
                } else {
                    ais_shape->SetColor(obj_color);
                }
                
                // Store shape data
                ShapeData shape_data;
                shape_data.ais_shape = ais_shape;
                shape_data.topo_shape = shape;
                shape_data.color.set_r(obj_color.Red());
                shape_data.color.set_g(obj_color.Green());
                shape_data.color.set_b(obj_color.Blue());
                shape_data.color.set_a(1.0);
                shape_data.shape_id = shape_id;
                
                shapes_[shape_id] = std::move(shape_data);
                shape_ids.push_back(shape_id);
                
                spdlog::info("importObjFileInternal: Successfully imported OBJ shape: {}", shape_id);
            }
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to import OBJ file '" + file_path + "': " + e.what());
    }
    
    return shape_ids;
}

// Additional format implementations would go here (PLY, GLTF, etc.)

std::string GeometryServiceImpl::detectFileFormat(const std::string& file_path, const std::string& force_format) {
    if (!force_format.empty()) {
        std::string upper_format = force_format;
        std::transform(upper_format.begin(), upper_format.end(), upper_format.begin(), ::toupper);
        return upper_format;
    }
    
    return detectFormatFromExtension(file_path);
}

std::string GeometryServiceImpl::detectFormatFromExtension(const std::string& filename) {
    // Extract extension
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "UNKNOWN";
    }
    
    std::string ext = filename.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    
    // Map extensions to formats
    if (ext == "STEP" || ext == "STP") return "STEP";
    if (ext == "IGES" || ext == "IGS") return "IGES";
    if (ext == "BREP" || ext == "BRP") return "BREP";
    if (ext == "STL") return "STL";
    if (ext == "OBJ") return "OBJ";
    if (ext == "PLY") return "PLY";
    if (ext == "GLTF") return "GLTF";
    if (ext == "GLB") return "GLB";
    if (ext == "VRML" || ext == "WRL") return "VRML";
    
    return "UNKNOWN";
}


geometry::ModelFileInfo GeometryServiceImpl::getModelFileInfo(
    const std::string& filename, const std::string& model_data, int shape_count, const std::string& format) {
    
    geometry::ModelFileInfo file_info;
    file_info.set_filename(filename);
    file_info.set_file_size(model_data.size());
    file_info.set_shape_count(shape_count);
    file_info.set_format(format);
    
    // Set current time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    file_info.set_creation_time(ss.str());
    
    // Set format version and supported features based on format
    // Fields removed in simplified proto - commented out for now
    // if (format == "STEP") {
    //     file_info.set_format_version("AP214");
    //     file_info.add_supported_features("geometry");
    //     file_info.add_supported_features("colors");
    //     file_info.add_supported_features("materials");
    //     file_info.add_supported_features("names");
    // } else if (format == "IGES") {
    //     file_info.set_format_version("2014");
    //     file_info.add_supported_features("geometry");
    //     file_info.add_supported_features("colors");
    // } else if (format == "STL") {
    //     file_info.set_format_version("ASCII/Binary");
    //     file_info.add_supported_features("geometry");
    // } else if (format == "OBJ") {
    //     file_info.set_format_version("Wavefront");
    //     file_info.add_supported_features("geometry");
    //     file_info.add_supported_features("materials");
    //     file_info.add_supported_features("textures");
    // } else if (format == "BREP") {
    //     file_info.set_format_version("OCCT Native");
    //     file_info.add_supported_features("geometry");
    //     file_info.add_supported_features("topology");
    // }
    
    return file_info;
}

std::vector<std::string> GeometryServiceImpl::importModelDataInternal(
    const std::string& model_data, const std::string& filename, const geometry::ModelImportOptions& options) {
    
    // For data import, we need to write to a temporary file first
    // This is a limitation of most OCCT readers that expect file paths
    
    std::string temp_filename = "/tmp/occt_import_" + std::to_string(std::time(nullptr)) + "_" + filename;
    
    try {
        // Write data to temporary file
        std::ofstream temp_file(temp_filename, std::ios::binary);
        if (!temp_file) {
            throw std::runtime_error("Failed to create temporary file: " + temp_filename);
        }
        temp_file.write(model_data.data(), model_data.size());
        temp_file.close();
        
        // Import from temporary file
        std::vector<std::string> shape_ids = importModelFileInternal(temp_filename, options);
        
        // Clean up temporary file
        std::filesystem::remove(temp_filename);
        
        return shape_ids;
        
    } catch (const std::exception& e) {
        // Clean up temporary file on error
        try {
            std::filesystem::remove(temp_filename);
        } catch (...) {
            // Ignore cleanup errors
        }
        throw;
    }
}

std::vector<std::string> GeometryServiceImpl::importPlyFileInternal(
    const std::string& file_path, const geometry::ModelImportOptions& options) {
    
    std::vector<std::string> shape_ids;
    
    // PLY format is not yet supported - just log and return empty vector
    spdlog::warn("importPlyFileInternal: PLY format import is not yet supported for file: {}", file_path);
    
    return shape_ids;
}

std::vector<std::string> GeometryServiceImpl::importGltfFileInternal(
    const std::string& file_path, const geometry::ModelImportOptions& options) {
    
    std::vector<std::string> shape_ids;
    
    // GLTF/GLB format is not yet supported - just log and return empty vector
    spdlog::warn("importGltfFileInternal: GLTF/GLB format import is not yet supported for file: {}", file_path);
    
    return shape_ids;
}

bool GeometryServiceImpl::exportModelFileInternal(
    const std::vector<std::string>& shape_ids, const geometry::ModelExportOptions& options,
    std::string& model_data, geometry::ModelFileInfo& file_info) {
    
    try {
        // Collect shapes to export
        std::vector<TopoDS_Shape> shapes_to_export;
        for (const auto& shape_id : shape_ids) {
            auto it = shapes_.find(shape_id);
            if (it != shapes_.end()) {
                shapes_to_export.push_back(it->second.topo_shape);
            }
        }
        
        if (shapes_to_export.empty()) {
            spdlog::error("exportModelFileInternal: No valid shapes found");
            return false;
        }
        
        // Create compound if multiple shapes
        TopoDS_Shape shape_to_write;
        if (shapes_to_export.size() == 1) {
            shape_to_write = shapes_to_export[0];
        } else {
            TopoDS_Compound compound;
            BRep_Builder builder;
            builder.MakeCompound(compound);
            for (const auto& shape : shapes_to_export) {
                builder.Add(compound, shape);
            }
            shape_to_write = compound;
        }
        
        std::string format = options.format();
        if (format.empty()) format = "STEP";
        
        // Export based on format
        if (format == "STEP" || format == "STP") {
            // Use STEPControl_Writer for STEP export
            STEPControl_Writer writer;
            IFSelect_ReturnStatus status = writer.Transfer(shape_to_write, STEPControl_AsIs);
            if (status != IFSelect_RetDone) {
                spdlog::error("exportModelFileInternal: Failed to transfer shape for STEP export");
                return false;
            }
            
            // Write to temporary file then read it back
            std::string temp_file = "temp_export.step";
            if (!writer.Write(temp_file.c_str())) {
                spdlog::error("exportModelFileInternal: Failed to write STEP file");
                return false;
            }
            
            // Read file content
            std::ifstream file(temp_file, std::ios::binary | std::ios::ate);
            if (!file) {
                spdlog::error("exportModelFileInternal: Failed to open temp file");
                return false;
            }
            
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            model_data.resize(file_size);
            file.read(&model_data[0], file_size);
            file.close();
            
            // Delete temp file
            std::remove(temp_file.c_str());
            
            // Set file info
            file_info.set_filename("model.step");
            file_info.set_file_size(file_size);
            file_info.set_shape_count(shapes_to_export.size());
            file_info.set_format("STEP");
            
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            file_info.set_creation_time(std::ctime(&time_t));
            
            return true;
            
        } else if (format == "BREP" || format == "BRP") {
            // Use BRepTools for BREP export
            std::stringstream ss;
            BRepTools::Write(shape_to_write, ss);
            model_data = ss.str();
            
            // Set file info
            file_info.set_filename("model.brep");
            file_info.set_file_size(model_data.size());
            file_info.set_shape_count(shapes_to_export.size());
            file_info.set_format("BREP");
            
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            file_info.set_creation_time(std::ctime(&time_t));
            
            return true;
            
        } else {
            spdlog::error("exportModelFileInternal: Unsupported format: {}", format);
            return false;
        }
        
    } catch (const std::exception& e) {
        spdlog::error("exportModelFileInternal: Exception: {}", e.what());
        return false;
    }
}