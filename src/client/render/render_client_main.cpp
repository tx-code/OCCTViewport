#include <iostream>
#include <memory>

#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "geometry_client.h"
#include "mesh_renderer.h"

class RenderClientApp {
public:
    RenderClientApp() : geometry_client_("localhost:50051") {
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    }
    
    bool Initialize() {
        spdlog::info("RenderClientApp: Initializing...");
        
        // Initialize GLFW
        if (!glfwInit()) {
            spdlog::error("RenderClientApp: Failed to initialize GLFW");
            return false;
        }
        
        // Set OpenGL version (3.3 Core)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        // Create window
        window_ = glfwCreateWindow(1280, 720, "OCCT gRPC Render Client", nullptr, nullptr);
        if (!window_) {
            spdlog::error("RenderClientApp: Failed to create GLFW window");
            glfwTerminate();
            return false;
        }
        
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1); // Enable vsync
        
        // Set window user pointer for callbacks
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, FramebufferSizeCallback);
        glfwSetMouseButtonCallback(window_, MouseButtonCallback);
        glfwSetCursorPosCallback(window_, CursorPosCallback);
        glfwSetScrollCallback(window_, ScrollCallback);
        
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        
        ImGui::StyleColorsDark();
        
        ImGui_ImplGlfw_InitForOpenGL(window_, true);
        ImGui_ImplOpenGL3_Init("#version 330 core");
        
        // Initialize mesh renderer
        mesh_renderer_ = std::make_unique<MeshRenderer>();
        if (!mesh_renderer_->Initialize()) {
            spdlog::error("RenderClientApp: Failed to initialize mesh renderer");
            return false;
        }
        
        // Connect to geometry server
        if (!geometry_client_.Connect()) {
            spdlog::error("RenderClientApp: Failed to connect to geometry server");
            return false;
        }
        
        spdlog::info("RenderClientApp: Initialization completed successfully");
        return true;
    }
    
    void Run() {
        spdlog::info("RenderClientApp: Starting main loop...");
        
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            
            // Start ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            // Create docking space
            ImGuiDockNodeFlags dock_flags = ImGuiDockNodeFlags_PassthruCentralNode;
            ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), dock_flags);
            
            // Render GUI
            RenderControlPanel();
            RenderViewport();
            
            // Render ImGui
            ImGui::Render();
            
            int display_w, display_h;
            glfwGetFramebufferSize(window_, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            glfwSwapBuffers(window_);
        }
    }
    
    void Shutdown() {
        spdlog::info("RenderClientApp: Shutting down...");
        
        if (mesh_renderer_) {
            mesh_renderer_->Shutdown();
            mesh_renderer_.reset();
        }
        
        geometry_client_.Disconnect();
        
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        if (window_) {
            glfwDestroyWindow(window_);
        }
        glfwTerminate();
    }
    
private:
    GLFWwindow* window_{nullptr};
    GeometryClient geometry_client_;
    std::unique_ptr<MeshRenderer> mesh_renderer_;
    
    // UI state
    bool show_wireframe_{false};
    bool auto_refresh_{false};
    float last_refresh_time_{0.0f};
    
    // Mouse interaction
    bool mouse_dragging_{false};
    double last_mouse_x_{0.0};
    double last_mouse_y_{0.0};
    
    void RenderControlPanel() {
        if (ImGui::Begin("Control Panel")) {
            // Connection status
            ImGui::Text("Connection: %s", geometry_client_.IsConnected() ? "Connected" : "Disconnected");
            
            if (ImGui::Button("Reconnect")) {
                if (!geometry_client_.IsConnected()) {
                    geometry_client_.Connect();
                }
            }
            
            ImGui::Separator();
            
            // Geometry operations
            ImGui::Text("Geometry Operations");
            
            if (ImGui::Button("Create Box")) {
                CreateRandomBox();
            }
            ImGui::SameLine();
            if (ImGui::Button("Create Cone")) {
                CreateRandomCone();
            }
            
            if (ImGui::Button("Create Demo Scene")) {
                geometry_client_.CreateDemoScene();
                RefreshMeshes();
            }
            
            if (ImGui::Button("Clear All")) {
                geometry_client_.ClearAll();
                mesh_renderer_->ClearAllMeshes();
            }
            
            ImGui::Separator();
            
            // Mesh operations
            ImGui::Text("Mesh Operations");
            
            if (ImGui::Button("Refresh Meshes")) {
                RefreshMeshes();
            }
            
            ImGui::Checkbox("Auto Refresh", &auto_refresh_);
            
            ImGui::Separator();
            
            // Rendering options
            ImGui::Text("Rendering");
            
            if (ImGui::Checkbox("Wireframe", &show_wireframe_)) {
                glPolygonMode(GL_FRONT_AND_BACK, show_wireframe_ ? GL_LINE : GL_FILL);
            }
            
            ImGui::Separator();
            
            // Statistics
            ImGui::Text("Statistics");
            ImGui::Text("Meshes: %d", mesh_renderer_->GetMeshCount());
            ImGui::Text("Triangles: %d", mesh_renderer_->GetTriangleCount());
            
            // Server info
            if (geometry_client_.IsConnected()) {
                auto system_info = geometry_client_.GetSystemInfo();
                ImGui::Text("Server Shapes: %d", system_info.active_shapes);
                ImGui::Text("Server Version: %s", system_info.version.c_str());
            }
            
            ImGui::Separator();
            
            // Camera controls
            ImGui::Text("Camera Controls");
            ImGui::Text("Left Mouse: Rotate");
            ImGui::Text("Right Mouse: Pan");
            ImGui::Text("Scroll: Zoom");
            
            if (ImGui::Button("Reset Camera")) {
                ResetCamera();
            }
        }
        ImGui::End();
    }
    
    void RenderViewport() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        
        if (ImGui::Begin("3D Viewport")) {
            ImVec2 viewport_size = ImGui::GetContentRegionAvail();
            
            if (viewport_size.x > 0 && viewport_size.y > 0) {
                // Update camera matrices
                float aspect_ratio = viewport_size.x / viewport_size.y;
                auto& camera = mesh_renderer_->GetCamera();
                
                glm::mat4 view = camera.GetViewMatrix();
                glm::mat4 projection = camera.GetProjectionMatrix(aspect_ratio);
                
                mesh_renderer_->SetViewMatrix(view);
                mesh_renderer_->SetProjectionMatrix(projection);
                
                // Create framebuffer for offscreen rendering
                // For now, render directly to the default framebuffer
                // TODO: Implement proper offscreen rendering
                
                // Set viewport
                glViewport(0, 0, static_cast<int>(viewport_size.x), static_cast<int>(viewport_size.y));
                
                // Render meshes
                mesh_renderer_->Render();
                
                // Display rendered image in ImGui
                // TODO: Capture the rendered image and display it as ImGui texture
                ImGui::Text("3D Viewport (%dx%d)", (int)viewport_size.x, (int)viewport_size.y);
            }
        }
        ImGui::End();
        
        ImGui::PopStyleVar();
    }
    
    void CreateRandomBox() {
        float x = static_cast<float>(rand()) / RAND_MAX * 100.0f - 50.0f;
        float y = static_cast<float>(rand()) / RAND_MAX * 100.0f - 50.0f;
        float z = 0.0f;
        float size = 10.0f + static_cast<float>(rand()) / RAND_MAX * 20.0f;
        
        std::string shape_id = geometry_client_.CreateBox(x, y, z, size, size, size);
        if (!shape_id.empty()) {
            // Get mesh data and add to renderer
            auto mesh_data = geometry_client_.GetMeshData(shape_id);
            if (!mesh_data.vertices.empty()) {
                mesh_renderer_->AddMesh(mesh_data.shape_id, mesh_data.vertices,
                                       mesh_data.normals, mesh_data.indices, mesh_data.color);
            }
        }
    }
    
    void CreateRandomCone() {
        float x = static_cast<float>(rand()) / RAND_MAX * 100.0f - 50.0f;
        float y = static_cast<float>(rand()) / RAND_MAX * 100.0f - 50.0f;
        float z = 0.0f;
        float base_radius = 5.0f + static_cast<float>(rand()) / RAND_MAX * 15.0f;
        float top_radius = static_cast<float>(rand()) / RAND_MAX * base_radius * 0.5f;
        float height = 10.0f + static_cast<float>(rand()) / RAND_MAX * 30.0f;
        
        std::string shape_id = geometry_client_.CreateCone(x, y, z, base_radius, top_radius, height);
        if (!shape_id.empty()) {
            // Get mesh data and add to renderer
            auto mesh_data = geometry_client_.GetMeshData(shape_id);
            if (!mesh_data.vertices.empty()) {
                mesh_renderer_->AddMesh(mesh_data.shape_id, mesh_data.vertices,
                                       mesh_data.normals, mesh_data.indices, mesh_data.color);
            }
        }
    }
    
    void RefreshMeshes() {
        if (!geometry_client_.IsConnected()) {
            return;
        }
        
        spdlog::info("RenderClientApp: Refreshing meshes...");
        
        // Clear existing meshes
        mesh_renderer_->ClearAllMeshes();
        
        // Get all meshes from server
        auto all_meshes = geometry_client_.GetAllMeshes();
        
        for (const auto& mesh_data : all_meshes) {
            if (!mesh_data.vertices.empty()) {
                mesh_renderer_->AddMesh(mesh_data.shape_id, mesh_data.vertices,
                                       mesh_data.normals, mesh_data.indices, mesh_data.color);
            }
        }
        
        spdlog::info("RenderClientApp: Refreshed {} meshes", all_meshes.size());
    }
    
    void ResetCamera() {
        auto& camera = mesh_renderer_->GetCamera();
        camera.position = glm::vec3(0.0f, -100.0f, 50.0f);
        camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
        camera.up = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    
    // GLFW callbacks
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
    }
    
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        auto* app = static_cast<RenderClientApp*>(glfwGetWindowUserPointer(window));
        
        if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT) {
            app->mouse_dragging_ = (action == GLFW_PRESS);
            if (app->mouse_dragging_) {
                glfwGetCursorPos(window, &app->last_mouse_x_, &app->last_mouse_y_);
            }
        }
    }
    
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* app = static_cast<RenderClientApp*>(glfwGetWindowUserPointer(window));
        
        if (app->mouse_dragging_) {
            double delta_x = xpos - app->last_mouse_x_;
            double delta_y = ypos - app->last_mouse_y_;
            
            int button_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
            if (button_state == GLFW_PRESS) {
                // Rotate camera
                auto& camera = app->mesh_renderer_->GetCamera();
                camera.RotateAroundTarget(static_cast<float>(delta_x) * 0.01f, 
                                         static_cast<float>(delta_y) * 0.01f);
            }
            
            button_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
            if (button_state == GLFW_PRESS) {
                // Pan camera
                auto& camera = app->mesh_renderer_->GetCamera();
                camera.Pan(static_cast<float>(delta_x) * 0.1f, 
                          static_cast<float>(-delta_y) * 0.1f);
            }
            
            app->last_mouse_x_ = xpos;
            app->last_mouse_y_ = ypos;
        }
    }
    
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        auto* app = static_cast<RenderClientApp*>(glfwGetWindowUserPointer(window));
        
        // Zoom camera
        auto& camera = app->mesh_renderer_->GetCamera();
        camera.Zoom(static_cast<float>(-yoffset) * 5.0f);
    }
};

int main() {
    spdlog::info("OCCT gRPC Render Client with 3D Rendering starting...");
    
    RenderClientApp app;
    
    if (!app.Initialize()) {
        spdlog::error("Failed to initialize application");
        return 1;
    }
    
    try {
        app.Run();
    } catch (const std::exception& e) {
        spdlog::error("Application error: {}", e.what());
        app.Shutdown();
        return 1;
    }
    
    app.Shutdown();
    return 0;
}