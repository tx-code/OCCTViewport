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

// Standard includes
#include <sstream>
#include <fstream>
#include <chrono>
#include <iomanip>
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
    
    spdlog::info("GeometryService: OCCT context initialized successfully");
}

GeometryServiceImpl::~GeometryServiceImpl() {
    spdlog::info("GeometryService: Shutting down...");
}

std::string GeometryServiceImpl::generateShapeId() {
    return "shape_" + std::to_string(shape_counter_.fetch_add(1));
}

grpc::Status GeometryServiceImpl::CreateBox(grpc::ServerContext* context,
                                           const geometry::BoxRequest* request,
                                           geometry::ShapeResponse* response) {
    try {
        spdlog::info("CreateBox: width={}, height={}, depth={}", 
                    request->width(), request->height(), request->depth());
        
        std::string shape_id = generateShapeId();
        Handle(AIS_Shape) ais_shape = createBoxShape(*request);
        
        if (ais_shape.IsNull()) {
            response->set_success(false);
            response->set_message("Failed to create box shape");
            return grpc::Status::OK;
        }
        
        // Store shape data
        ShapeData shape_data;
        shape_data.ais_shape = ais_shape;
        shape_data.topo_shape = ais_shape->Shape();
        shape_data.color = request->color();
        shape_data.shape_id = shape_id;
        
        shapes_[shape_id] = std::move(shape_data);
        
        // Set response
        response->set_shape_id(shape_id);
        response->set_success(true);
        response->set_message("Box created successfully");
        
        // Set shape properties in response
        auto* properties = response->mutable_properties();
        properties->set_shape_id(shape_id);
        properties->set_visible(true);
        properties->set_selected(false);
        properties->set_highlighted(false);
        *properties->mutable_color() = request->color();
        
        spdlog::info("CreateBox: Successfully created box with ID: {}", shape_id);
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
        spdlog::info("CreateCone: base_radius={}, top_radius={}, height={}", 
                    request->base_radius(), request->top_radius(), request->height());
        
        std::string shape_id = generateShapeId();
        Handle(AIS_Shape) ais_shape = createConeShape(*request);
        
        if (ais_shape.IsNull()) {
            response->set_success(false);
            response->set_message("Failed to create cone shape");
            return grpc::Status::OK;
        }
        
        // Store shape data
        ShapeData shape_data;
        shape_data.ais_shape = ais_shape;
        shape_data.topo_shape = ais_shape->Shape();
        shape_data.color = request->color();
        shape_data.shape_id = shape_id;
        
        shapes_[shape_id] = std::move(shape_data);
        
        // Set response
        response->set_shape_id(shape_id);
        response->set_success(true);
        response->set_message("Cone created successfully");
        
        // Set shape properties
        auto* properties = response->mutable_properties();
        properties->set_shape_id(shape_id);
        properties->set_visible(true);
        properties->set_selected(false);
        properties->set_highlighted(false);
        *properties->mutable_color() = request->color();
        
        spdlog::info("CreateCone: Successfully created cone with ID: {}", shape_id);
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
    response->set_version("1.0.0");
    response->set_active_shapes(static_cast<int32_t>(shapes_.size()));
    response->set_occt_version("7.8.0"); // Update with actual OCCT version
    
    spdlog::info("GetSystemInfo: Returning system information");
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::ListShapes(grpc::ServerContext* context,
                                            const geometry::EmptyRequest* request,
                                            geometry::ShapeListResponse* response) {
    for (const auto& [shape_id, shape_data] : shapes_) {
        response->add_shape_ids(shape_id);
    }
    response->set_total_count(static_cast<int32_t>(shapes_.size()));
    
    spdlog::info("ListShapes: Returning {} shapes", shapes_.size());
    return grpc::Status::OK;
}

// Placeholder implementations for remaining methods
grpc::Status GeometryServiceImpl::CreateSphere(grpc::ServerContext* context,
                                              const geometry::SphereRequest* request,
                                              geometry::ShapeResponse* response) {
    response->set_success(false);
    response->set_message("CreateSphere not implemented yet");
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::CreateCylinder(grpc::ServerContext* context,
                                                const geometry::CylinderRequest* request,
                                                geometry::ShapeResponse* response) {
    response->set_success(false);
    response->set_message("CreateCylinder not implemented yet");
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::DeleteShape(grpc::ServerContext* context,
                                             const geometry::ShapeRequest* request,
                                             geometry::StatusResponse* response) {
    response->set_success(false);
    response->set_message("DeleteShape not implemented yet");
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
    
    std::string shape_id = request->shape_id();
    auto it = shapes_.find(shape_id);
    if (it == shapes_.end()) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Shape not found: " + shape_id);
    }
    
    try {
        spdlog::info("GetMeshData: Extracting mesh for shape: {}", shape_id);
        *response = extractMeshData(shape_id);
        spdlog::info("GetMeshData: Successfully extracted mesh with {} vertices, {} indices", 
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
        spdlog::info("GetAllMeshes: Streaming {} shapes", shapes_.size());
        
        for (const auto& [shape_id, shape_data] : shapes_) {
            geometry::MeshData mesh_data = extractMeshData(shape_id);
            if (!writer->Write(mesh_data)) {
                spdlog::error("GetAllMeshes: Failed to write mesh data for shape: {}", shape_id);
                break;
            }
            spdlog::info("GetAllMeshes: Sent mesh for shape: {} ({} vertices)", 
                        shape_id, mesh_data.vertices_size());
        }
        
        spdlog::info("GetAllMeshes: Completed streaming all meshes");
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        spdlog::error("GetAllMeshes: Exception occurred: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to stream mesh data: " + std::string(e.what()));
    }
}

grpc::Status GeometryServiceImpl::GetShapeProperties(grpc::ServerContext* context,
                                                    const geometry::ShapeRequest* request,
                                                    geometry::ShapeProperties* response) {
    response->set_shape_id("placeholder");
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::Subscribe(grpc::ServerContext* context,
                                           grpc::ServerReaderWriter<geometry::ServerEvent, geometry::ClientEvent>* stream) {
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::ClearAll(grpc::ServerContext* context,
                                          const geometry::EmptyRequest* request,
                                          geometry::StatusResponse* response) {
    shapes_.clear();
    response->set_success(true);
    response->set_message("All shapes cleared");
    spdlog::info("ClearAll: All shapes cleared");
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::CreateDemoScene(grpc::ServerContext* context,
                                                 const geometry::EmptyRequest* request,
                                                 geometry::StatusResponse* response) {
    try {
        spdlog::info("CreateDemoScene: Creating demo geometry objects...");
        
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
                spdlog::info("CreateDemoScene: Created demo box with ID: {}", box_response.shape_id());
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
                spdlog::info("CreateDemoScene: Created demo cone with ID: {}", cone_response.shape_id());
            }
        }
        
        response->set_success(true);
        response->set_message("Demo scene created successfully. Created " + std::to_string(created_count) + " objects.");
        
        spdlog::info("CreateDemoScene: Successfully created demo scene with {} objects", created_count);
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
    mesh_data.set_version(1);
    
    auto it = shapes_.find(shape_id);
    if (it == shapes_.end()) {
        spdlog::error("extractMeshData: Shape not found: {}", shape_id);
        return mesh_data;
    }
    
    const ShapeData& shape_data = it->second;
    const TopoDS_Shape& shape = shape_data.topo_shape;
    
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
    
    // Set color
    *mesh_data.mutable_color() = shape_data.color;
    
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
        *bounding_box->mutable_min() = toProtoPoint(min_pt);
        *bounding_box->mutable_max() = toProtoPoint(max_pt);
    }
    
    spdlog::info("extractMeshData: Generated mesh for {}: {} vertices, {} triangles", 
                shape_id, vertices.size(), indices.size() / 3);
    
    return mesh_data;
}

// ========== STEP File Operations ==========

grpc::Status GeometryServiceImpl::ImportStepFile(grpc::ServerContext* context,
                                                 const geometry::StepFileRequest* request,
                                                 geometry::StepImportResponse* response) {
    spdlog::info("ImportStepFile: Importing file: {}", request->file_path());
    
    try {
        std::vector<std::string> shape_ids = importStepFileInternal(
            request->file_path(), request->options());
        
        response->set_success(true);
        response->set_message("STEP file imported successfully");
        
        for (const auto& shape_id : shape_ids) {
            response->add_shape_ids(shape_id);
        }
        
        // Set file info
        geometry::StepFileInfo file_info = getStepFileInfo(
            request->file_path(), "", shape_ids.size());
        *response->mutable_file_info() = file_info;
        
        spdlog::info("ImportStepFile: Successfully imported {} shapes from {}", 
                    shape_ids.size(), request->file_path());
        
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("Failed to import STEP file: ") + e.what());
        spdlog::error("ImportStepFile: Exception: {}", e.what());
    }
    
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::LoadStepFromData(grpc::ServerContext* context,
                                                  const geometry::StepDataRequest* request,
                                                  geometry::StepImportResponse* response) {
    spdlog::info("LoadStepFromData: Loading STEP data, size: {} bytes", request->step_data().size());
    
    try {
        std::string step_data_str(request->step_data().begin(), request->step_data().end());
        std::vector<std::string> shape_ids = importStepDataInternal(
            step_data_str, request->filename(), request->options());
        
        response->set_success(true);
        response->set_message("STEP data loaded successfully");
        
        for (const auto& shape_id : shape_ids) {
            response->add_shape_ids(shape_id);
        }
        
        // Set file info
        geometry::StepFileInfo file_info = getStepFileInfo(
            request->filename(), step_data_str, shape_ids.size());
        *response->mutable_file_info() = file_info;
        
        spdlog::info("LoadStepFromData: Successfully loaded {} shapes from data", shape_ids.size());
        
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("Failed to load STEP data: ") + e.what());
        spdlog::error("LoadStepFromData: Exception: {}", e.what());
    }
    
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::ExportStepFile(grpc::ServerContext* context,
                                                 const geometry::StepExportRequest* request,
                                                 geometry::StepFileResponse* response) {
    spdlog::info("ExportStepFile: Exporting {} shapes", request->shape_ids_size());
    
    try {
        std::vector<std::string> shape_ids;
        for (int i = 0; i < request->shape_ids_size(); ++i) {
            shape_ids.push_back(request->shape_ids(i));
        }
        
        std::string step_data;
        geometry::StepFileInfo file_info;
        
        bool success = exportStepFileInternal(shape_ids, request->options(), step_data, file_info);
        
        if (success) {
            response->set_success(true);
            response->set_message("STEP file exported successfully");
            response->set_step_data(step_data);
            response->set_filename(file_info.filename());
            *response->mutable_file_info() = file_info;
            
            spdlog::info("ExportStepFile: Successfully exported {} shapes, size: {} bytes", 
                        shape_ids.size(), step_data.size());
        } else {
            response->set_success(false);
            response->set_message("Failed to export STEP file");
        }
        
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("Failed to export STEP file: ") + e.what());
        spdlog::error("ExportStepFile: Exception: {}", e.what());
    }
    
    return grpc::Status::OK;
}

// ========== STEP File Helper Methods ==========

std::vector<std::string> GeometryServiceImpl::importStepFileInternal(
    const std::string& file_path, const geometry::StepImportOptions& options) {
    
    std::vector<std::string> shape_ids;
    
    spdlog::info("importStepFileInternal: Processing file: {}", file_path);
    
    // Create XCAF application and document
    Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) doc;
    app->NewDocument("MDTV-XCAF", doc);
    
    Handle(XCAFDoc_ShapeTool) shape_tool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    Handle(XCAFDoc_ColorTool) color_tool = XCAFDoc_DocumentTool::ColorTool(doc->Main());
    
    // Initialize STEP reader
    STEPCAFControl_Reader reader;
    
    // Note: Precision configuration will be added later after confirming OCCT API
    
    // Read the file
    IFSelect_ReturnStatus status = reader.ReadFile(file_path.c_str());
    if (status != IFSelect_RetDone) {
        throw std::runtime_error("Failed to read STEP file: " + file_path);
    }
    
    // Transfer to document
    if (!reader.Transfer(doc)) {
        throw std::runtime_error("Failed to transfer STEP data to document");
    }
    
    // Get free shapes from the document
    TDF_LabelSequence shape_labels;
    shape_tool->GetFreeShapes(shape_labels);
    
    spdlog::info("importStepFileInternal: Found {} free shapes", shape_labels.Length());
    
    // Process each shape
    for (int i = 1; i <= shape_labels.Length(); ++i) {
        TDF_Label shape_label = shape_labels.Value(i);
        TopoDS_Shape shape = shape_tool->GetShape(shape_label);
        
        if (!shape.IsNull()) {
            // Generate shape ID
            std::string shape_id = generateShapeId();
            
            // Create AIS shape
            Handle(AIS_Shape) ais_shape = new AIS_Shape(shape);
            
            // Set color if available and requested
            if (options.import_colors() && color_tool->IsSet(shape_label, XCAFDoc_ColorGen)) {
                Quantity_Color color;
                if (color_tool->GetColor(shape_label, XCAFDoc_ColorGen, color)) {
                    ais_shape->SetColor(color);
                }
            }
            
            // Set name if available and requested
            if (options.import_names()) {
                Handle(TDataStd_Name) name_attr;
                if (shape_label.FindAttribute(TDataStd_Name::GetID(), name_attr)) {
                    TCollection_ExtendedString name = name_attr->Get();
                    TCollection_AsciiString ascii_name(name);
                    spdlog::info("importStepFileInternal: Shape {} has name: {}", 
                               shape_id, ascii_name.ToCString());
                }
            }
            
            // Store shape data
            ShapeData shape_data;
            shape_data.ais_shape = ais_shape;
            shape_data.topo_shape = shape;
            shape_data.shape_id = shape_id;
            Quantity_Color default_color;
            ais_shape->Color(default_color);
            shape_data.color = toProtoColor(default_color);
            shape_data.visible = true;
            
            shapes_[shape_id] = shape_data;
            
            // Display in context
            context_->Display(ais_shape, Standard_False);
            
            shape_ids.push_back(shape_id);
            
            spdlog::info("importStepFileInternal: Created shape {}", shape_id);
        }
    }
    
    // Update display
    context_->UpdateCurrentViewer();
    
    app->Close(doc);
    
    return shape_ids;
}

std::vector<std::string> GeometryServiceImpl::importStepDataInternal(
    const std::string& step_data, const std::string& filename, 
    const geometry::StepImportOptions& options) {
    
    // Write step data to temporary file
    std::string temp_filename = "temp_" + filename;
    std::ofstream temp_file(temp_filename, std::ios::binary);
    if (!temp_file) {
        throw std::runtime_error("Failed to create temporary file: " + temp_filename);
    }
    
    temp_file.write(step_data.c_str(), step_data.size());
    temp_file.close();
    
    std::vector<std::string> shape_ids;
    
    try {
        // Import from temporary file
        shape_ids = importStepFileInternal(temp_filename, options);
        
        // Clean up temporary file
        std::remove(temp_filename.c_str());
        
    } catch (...) {
        // Clean up temporary file even on exception
        std::remove(temp_filename.c_str());
        throw;
    }
    
    return shape_ids;
}

bool GeometryServiceImpl::exportStepFileInternal(
    const std::vector<std::string>& shape_ids,
    const geometry::StepExportOptions& options,
    std::string& step_data,
    geometry::StepFileInfo& file_info) {
    
    if (shape_ids.empty()) {
        throw std::runtime_error("No shapes to export");
    }
    
    spdlog::info("exportStepFileInternal: Exporting {} shapes", shape_ids.size());
    
    // Create XCAF application and document
    Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) doc;
    app->NewDocument("MDTV-XCAF", doc);
    
    Handle(XCAFDoc_ShapeTool) shape_tool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    Handle(XCAFDoc_ColorTool) color_tool = XCAFDoc_DocumentTool::ColorTool(doc->Main());
    
    // Add shapes to document
    int valid_shapes = 0;
    for (const std::string& shape_id : shape_ids) {
        auto it = shapes_.find(shape_id);
        if (it != shapes_.end() && !it->second.topo_shape.IsNull()) {
            TDF_Label shape_label = shape_tool->AddShape(it->second.topo_shape);
            
            // Set color if requested
            if (options.export_colors()) {
                Quantity_Color color = fromProtoColor(it->second.color);
                color_tool->SetColor(shape_label, color, XCAFDoc_ColorGen);
            }
            
            // Set name if requested
            if (options.export_names()) {
                Handle(TDataStd_Name) name_attr = TDataStd_Name::Set(shape_label, shape_id.c_str());
            }
            
            valid_shapes++;
        }
    }
    
    if (valid_shapes == 0) {
        app->Close(doc);
        throw std::runtime_error("No valid shapes found for export");
    }
    
    // Initialize STEP writer
    STEPCAFControl_Writer writer;
    
    // Set schema version
    std::string schema = options.schema_version();
    if (schema.empty()) {
        schema = "AP214";
    }
    
    // Set units
    std::string units = options.units();
    if (units.empty()) {
        units = "mm";
    }
    
    // Transfer document to writer
    if (!writer.Transfer(doc, STEPControl_AsIs)) {
        app->Close(doc);
        throw std::runtime_error("Failed to transfer document to STEP writer");
    }
    
    // Write to temporary file
    std::string temp_filename = "export_temp.step";
    IFSelect_ReturnStatus status = writer.Write(temp_filename.c_str());
    
    if (status != IFSelect_RetDone) {
        app->Close(doc);
        std::remove(temp_filename.c_str());
        throw std::runtime_error("Failed to write STEP file");
    }
    
    // Read temporary file content
    std::ifstream temp_file(temp_filename, std::ios::binary);
    if (!temp_file) {
        app->Close(doc);
        std::remove(temp_filename.c_str());
        throw std::runtime_error("Failed to read exported STEP file");
    }
    
    step_data.assign(std::istreambuf_iterator<char>(temp_file),
                    std::istreambuf_iterator<char>());
    temp_file.close();
    
    // Clean up
    std::remove(temp_filename.c_str());
    app->Close(doc);
    
    // Set file info
    file_info.set_filename("export.step");
    file_info.set_file_size(step_data.size());
    file_info.set_shape_count(valid_shapes);
    file_info.set_schema_version(schema);
    
    // Set creation time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    file_info.set_creation_time(ss.str());
    
    spdlog::info("exportStepFileInternal: Successfully exported {} shapes, size: {} bytes", 
                valid_shapes, step_data.size());
    
    return true;
}

geometry::StepFileInfo GeometryServiceImpl::getStepFileInfo(
    const std::string& filename, const std::string& step_data, int shape_count) {
    
    geometry::StepFileInfo info;
    info.set_filename(filename);
    info.set_shape_count(shape_count);
    
    if (!step_data.empty()) {
        info.set_file_size(step_data.size());
    } else {
        // Try to get file size from disk
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (file) {
            info.set_file_size(file.tellg());
        }
    }
    
    // Set creation time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    info.set_creation_time(ss.str());
    
    info.set_schema_version("AP214");
    
    return info;
}

// BREP file operations implementation

grpc::Status GeometryServiceImpl::ImportBrepFile(grpc::ServerContext* context,
                                                  const geometry::BrepFileRequest* request,
                                                  geometry::BrepImportResponse* response) {
    spdlog::info("ImportBrepFile: Importing file: {}", request->file_path());
    
    try {
        std::vector<std::string> shape_ids = importBrepFileInternal(request->file_path(), request->options());
        
        response->set_success(true);
        response->set_message("BREP file imported successfully");
        
        for (const auto& shape_id : shape_ids) {
            response->add_shape_ids(shape_id);
        }
        
        geometry::BrepFileInfo file_info = getBrepFileInfo(request->file_path(), "", shape_ids.size());
        *response->mutable_file_info() = file_info;
        
        spdlog::info("ImportBrepFile: Successfully imported {} shapes from {}", 
                    shape_ids.size(), request->file_path());
        
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("Failed to import BREP file: ") + e.what());
        spdlog::error("ImportBrepFile: Exception: {}", e.what());
    }
    
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::LoadBrepFromData(grpc::ServerContext* context,
                                                    const geometry::BrepDataRequest* request,
                                                    geometry::BrepImportResponse* response) {
    spdlog::info("LoadBrepFromData: Loading BREP data, size: {} bytes", request->brep_data().size());
    
    try {
        std::string brep_data_str(request->brep_data().begin(), request->brep_data().end());
        std::vector<std::string> shape_ids = importBrepDataInternal(
            brep_data_str, request->filename(), request->options());
        
        response->set_success(true);
        response->set_message("BREP data loaded successfully");
        
        for (const auto& shape_id : shape_ids) {
            response->add_shape_ids(shape_id);
        }
        
        geometry::BrepFileInfo file_info = getBrepFileInfo(request->filename(), brep_data_str, shape_ids.size());
        *response->mutable_file_info() = file_info;
        
        spdlog::info("LoadBrepFromData: Successfully loaded {} shapes from data", shape_ids.size());
        
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("Failed to load BREP data: ") + e.what());
        spdlog::error("LoadBrepFromData: Exception: {}", e.what());
    }
    
    return grpc::Status::OK;
}

grpc::Status GeometryServiceImpl::ExportBrepFile(grpc::ServerContext* context,
                                                  const geometry::BrepExportRequest* request,
                                                  geometry::BrepFileResponse* response) {
    spdlog::info("ExportBrepFile: Exporting {} shapes", request->shape_ids_size());
    
    try {
        std::vector<std::string> shape_ids;
        for (int i = 0; i < request->shape_ids_size(); ++i) {
            shape_ids.push_back(request->shape_ids(i));
        }
        
        std::string brep_data;
        geometry::BrepFileInfo file_info;
        
        bool success = exportBrepFileInternal(shape_ids, request->options(), brep_data, file_info);
        
        if (success) {
            response->set_success(true);
            response->set_message("BREP file exported successfully");
            response->set_brep_data(brep_data);
            response->set_filename(file_info.filename());
            *response->mutable_file_info() = file_info;
            
            spdlog::info("ExportBrepFile: Successfully exported {} shapes", shape_ids.size());
        } else {
            response->set_success(false);
            response->set_message("Failed to export BREP file");
            spdlog::error("ExportBrepFile: Export failed");
        }
        
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("Failed to export BREP file: ") + e.what());
        spdlog::error("ExportBrepFile: Exception: {}", e.what());
    }
    
    return grpc::Status::OK;
}

// BREP file helper methods implementation

std::vector<std::string> GeometryServiceImpl::importBrepFileInternal(
    const std::string& file_path, const geometry::BrepImportOptions& options) {
    
    std::vector<std::string> shape_ids;
    
    BRep_Builder builder;
    TopoDS_Shape shape;
    
    try {
        BRepTools::Read(shape, file_path.c_str(), builder);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to read BREP file: " + file_path + " - " + e.what());
    }
    
    if (shape.IsNull()) {
        throw std::runtime_error("BREP file contains no valid shapes: " + file_path);
    }
    
    // Create AIS shape for visualization
    Handle(AIS_Shape) ais_shape = new AIS_Shape(shape);
    ais_shape->SetDisplayMode(AIS_Shaded);
    
    // Generate unique ID and store shape data
    std::string shape_id = generateShapeId();
    ShapeData shape_data;
    shape_data.ais_shape = ais_shape;
    shape_data.topo_shape = shape;
    shape_data.color = geometry::Color();
    shape_data.color.set_r(0.7);
    shape_data.color.set_g(0.7);
    shape_data.color.set_b(0.9);
    shape_data.color.set_a(1.0);
    shape_data.shape_id = shape_id;
    shape_data.visible = true;
    
    shapes_[shape_id] = shape_data;
    
    // Add to AIS context for rendering
    context_->Display(ais_shape, Standard_False);
    context_->SetColor(ais_shape, Quantity_Color(
        shape_data.color.r(), shape_data.color.g(), shape_data.color.b(), Quantity_TOC_RGB), Standard_False);
    
    shape_ids.push_back(shape_id);
    
    spdlog::info("ImportBrepFileInternal: Loaded shape {} from {}", shape_id, file_path);
    
    return shape_ids;
}

std::vector<std::string> GeometryServiceImpl::importBrepDataInternal(
    const std::string& brep_data, const std::string& filename, const geometry::BrepImportOptions& options) {
    
    std::vector<std::string> shape_ids;
    
    BRep_Builder builder;
    TopoDS_Shape shape;
    
    std::istringstream stream(brep_data);
    
    try {
        BRepTools::Read(shape, stream, builder);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to read BREP data from " + filename + " - " + e.what());
    }
    
    if (shape.IsNull()) {
        throw std::runtime_error("BREP data contains no valid shapes from " + filename);
    }
    
    // Create AIS shape for visualization
    Handle(AIS_Shape) ais_shape = new AIS_Shape(shape);
    ais_shape->SetDisplayMode(AIS_Shaded);
    
    // Generate unique ID and store shape data
    std::string shape_id = generateShapeId();
    ShapeData shape_data;
    shape_data.ais_shape = ais_shape;
    shape_data.topo_shape = shape;
    shape_data.color = geometry::Color();
    shape_data.color.set_r(0.7);
    shape_data.color.set_g(0.7);
    shape_data.color.set_b(0.9);
    shape_data.color.set_a(1.0);
    shape_data.shape_id = shape_id;
    shape_data.visible = true;
    
    shapes_[shape_id] = shape_data;
    
    // Add to AIS context for rendering
    context_->Display(ais_shape, Standard_False);
    context_->SetColor(ais_shape, Quantity_Color(
        shape_data.color.r(), shape_data.color.g(), shape_data.color.b(), Quantity_TOC_RGB), Standard_False);
    
    shape_ids.push_back(shape_id);
    
    spdlog::info("ImportBrepDataInternal: Loaded shape {} from data ({})", shape_id, filename);
    
    return shape_ids;
}

bool GeometryServiceImpl::exportBrepFileInternal(const std::vector<std::string>& shape_ids,
                                                 const geometry::BrepExportOptions& options,
                                                 std::string& brep_data,
                                                 geometry::BrepFileInfo& file_info) {
    
    if (shape_ids.empty()) {
        return false;
    }
    
    std::ostringstream stream;
    
    if (shape_ids.size() == 1) {
        // Single shape export
        auto it = shapes_.find(shape_ids[0]);
        if (it == shapes_.end()) {
            spdlog::error("ExportBrepFileInternal: Shape not found: {}", shape_ids[0]);
            return false;
        }
        
        try {
            BRepTools::Write(it->second.topo_shape, stream);
        } catch (const std::exception& e) {
            spdlog::error("ExportBrepFileInternal: Failed to write shape: {} - {}", shape_ids[0], e.what());
            return false;
        }
    } else {
        // Multiple shapes export
        if (options.export_as_compound()) {
            // Export as compound
            TopoDS_Compound compound;
            BRep_Builder builder;
            builder.MakeCompound(compound);
            
            for (const auto& shape_id : shape_ids) {
                auto it = shapes_.find(shape_id);
                if (it != shapes_.end()) {
                    builder.Add(compound, it->second.topo_shape);
                }
            }
            try {
                BRepTools::Write(compound, stream);
            } catch (const std::exception& e) {
                spdlog::error("ExportBrepFileInternal: Failed to write compound - {}", e.what());
                return false;
            }
        } else {
            // Export first shape only for now (could be extended to export multiple files)
            auto it = shapes_.find(shape_ids[0]);
            if (it != shapes_.end()) {
                try {
                    BRepTools::Write(it->second.topo_shape, stream);
                } catch (const std::exception& e) {
                    spdlog::error("ExportBrepFileInternal: Failed to write first shape: {} - {}", shape_ids[0], e.what());
                    return false;
                }
            }
        }
    }
    
    brep_data = stream.str();
    
    // Generate file info
    file_info = getBrepFileInfo("exported_shapes.brep", brep_data, shape_ids.size());
    
    return true;
}

geometry::BrepFileInfo GeometryServiceImpl::getBrepFileInfo(
    const std::string& filename, const std::string& brep_data, int shape_count) {
    
    geometry::BrepFileInfo info;
    info.set_filename(filename);
    info.set_shape_count(shape_count);
    
    if (!brep_data.empty()) {
        info.set_file_size(brep_data.size());
    } else {
        // Try to get file size from disk
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (file) {
            info.set_file_size(file.tellg());
        }
    }
    
    // Set creation time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    info.set_creation_time(ss.str());
    
    // Set BREP format version (OCCT uses different versions)
    info.set_format_version("OCCT_7.6");
    
    return info;
}