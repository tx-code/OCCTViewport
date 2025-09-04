// MIT License
//
// Copyright(c) 2025 Xing Tang <tang.xing1@outlook.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "OcctRenderClient.h"
#include "../grpc/geometry_client.h"
#include "../ui/grpc_performance_panel.h"

// ImGui
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

// OCCT
#include <AIS_AnimationCamera.hxx>
#include <AIS_DisplayMode.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_Triangulation.hxx>
#include <AIS_ViewCube.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_Handle.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <ElSLib.hxx>
#include <GeomAbs_Shape.hxx>
#include <Graphic3d_AspectFillArea3d.hxx>
#include <Graphic3d_MaterialAspect.hxx>
#include <OpenGl_Context.hxx>
#include <OpenGl_FrameBuffer.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <OpenGl_Texture.hxx>
#include <Poly_Triangulation.hxx>
#include <ProjLib.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_PointAspect.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <Prs3d_TypeOfHLR.hxx>
#include <Prs3d_TypeOfHighlight.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_NameOfColor.hxx>
#include <SelectMgr_PickingStrategy.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <V3d_AmbientLight.hxx>
#include <V3d_DirectionalLight.hxx>
#include <V3d_Light.hxx>
#include <V3d_TypeOfView.hxx>
#include <V3d_View.hxx>

// Others
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <future>
#include <cmath>

// Platform specific includes for Aspect_Window
#if defined(_WIN32)
#include <WNT_Window.hxx>
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#elif defined(__APPLE__)
#include <Cocoa_Window.hxx>
#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#else // Linux/X11
#include <Xw_Window.hxx>
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#endif
#include <GLFW/glfw3native.h> // For native access

// Definition of the PIMPL struct
struct OcctRenderClient::ViewInternal {
  //! GLFW window
  GLFWwindow *glfwWindow{nullptr};
  //! OCCT's wrapper for the window
  Handle(Aspect_Window) occtAspectWindow;
  //! OCCT 3D Viewer
  Handle(V3d_Viewer) viewer;
  //! OCCT 3D View
  Handle(V3d_View) view;
  //! 3D position in the view (converted from screen coordinates)
  gp_Pnt positionInView;
  //! OCCT Interactive Context
  Handle(AIS_InteractiveContext) context;

  //! AIS ViewCube for scene orientation
  Handle(AIS_ViewCube) viewCube;
  //! If true, ViewCube animation completes in a single update
  bool fixedViewCubeAnimationLoop{true};

  //! OpenGL graphics context
  Handle(OpenGl_Context) glContext;
  //! Framebuffer for offscreen rendering
  Handle(OpenGl_FrameBuffer) offscreenFBO;
  //! Initial width of the offscreen render target
  int renderWidth{800};
  //! Initial height of the offscreen render target
  int renderHeight{600};
  //! Flag to indicate if the FBO needs resizing
  bool needToResizeFBO{false};
  //! Dimensions of the ImGui viewport displaying the render
  ImVec2 viewport{0.0f, 0.0f};
  //! Position of the ImGui viewport displaying the render
  ImVec2 viewPos{0.0f, 0.0f};
  //! True if the ImGui window containing the render has focus
  bool renderWindowHasFocus{false};

  /// Visual properties
  //! Face color
  Quantity_Color faceColor{Quantity_NOC_GRAY90};
  //! Edge color
  Quantity_Color edgeColor{Quantity_NOC_BLACK};
  //! Edge width
  double boundaryEdgeWidth{2.0};
  //! Vertex color
  Quantity_Color vertexColor{Quantity_NOC_BLACK};
  //! Vertex size
  double vertexSize{2.0};

  //! Selection color
  Quantity_Color selectionColor{Quantity_NOC_RED};
  //! Highlight color
  Quantity_Color highlightColor{Quantity_NOC_YELLOW};

  // gRPC Geometry Client
  //! Geometry client for remote CAD operations
  std::unique_ptr<GeometryClient> geometryClient;
  
  // DPI/Content Scale tracking
  float currentDpiScale{1.0f};
  float lastDpiScale{1.0f};
  
  // UI state for gRPC integration
  bool showGrpcControlPanel{true};
  bool showGrpcPerformancePanel{false};
  bool autoRefreshMeshes{false};
  float lastRefreshTime{0.0f};
  
  // Performance monitoring panel
  std::unique_ptr<GrpcPerformancePanel> performancePanel;
  
  // Cache for system info to avoid excessive network calls
  GeometryClient::SystemInfo cachedSystemInfo;
  float lastSystemInfoUpdateTime{0.0f};
  static constexpr float SYSTEM_INFO_UPDATE_INTERVAL{1.0f}; // Update every 1 second
  bool hasValidSystemInfo{false};
  
  // Connection status tracking
  enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Error
  };
  ConnectionStatus connectionStatus{ConnectionStatus::Disconnected};
  std::string connectionErrorMessage;
  float lastConnectionAttemptTime{0.0f};
  static constexpr float RECONNECT_INTERVAL{5.0f}; // Retry connection every 5 seconds
  
  // Async connection handling
  std::future<bool> connectionFuture;
  std::atomic<bool> isConnecting{false};
  
  // Thread safety for resource management
  std::atomic<bool> isShuttingDown{false};
  mutable std::mutex geometryClientMutex;
};

namespace { // Anonymous namespace for static helper functions from original
            // file
//! Convert GLFW mouse button into Aspect_VKeyMouse.
static Aspect_VKeyMouse mouseButtonFromGlfw(int theButton) {
  switch (theButton) {
  case GLFW_MOUSE_BUTTON_LEFT:
    return Aspect_VKeyMouse_LeftButton;
  case GLFW_MOUSE_BUTTON_RIGHT:
    return Aspect_VKeyMouse_RightButton;
  case GLFW_MOUSE_BUTTON_MIDDLE:
    return Aspect_VKeyMouse_MiddleButton;
  }
  return Aspect_VKeyMouse_NONE;
}

//! Convert GLFW key modifiers into Aspect_VKeyFlags.
static Aspect_VKeyFlags keyFlagsFromGlfw(int theFlags) {
  Aspect_VKeyFlags aFlags = Aspect_VKeyFlags_NONE;
  if ((theFlags & GLFW_MOD_SHIFT) != 0) {
    aFlags |= Aspect_VKeyFlags_SHIFT;
  }
  if ((theFlags & GLFW_MOD_CONTROL) != 0) {
    aFlags |= Aspect_VKeyFlags_CTRL;
  }
  if ((theFlags & GLFW_MOD_ALT) != 0) {
    aFlags |= Aspect_VKeyFlags_ALT;
  }
  if ((theFlags & GLFW_MOD_SUPER) != 0) {
    aFlags |= Aspect_VKeyFlags_META;
  }
  return aFlags;
}

//! Create an opengl driver
Handle(OpenGl_GraphicDriver)
    createOpenGlDriver(const Handle(Aspect_DisplayConnection) &
                           displayConnection,
                       bool glDebug = false) {
  Handle(OpenGl_GraphicDriver) aGraphicDriver =
      new OpenGl_GraphicDriver(displayConnection, Standard_False);
  aGraphicDriver->ChangeOptions().buffersNoSwap = Standard_True;
  // contextCompatible is needed to support line thickness
  aGraphicDriver->ChangeOptions().contextCompatible = !glDebug;
  aGraphicDriver->ChangeOptions().ffpEnable = false;
  aGraphicDriver->ChangeOptions().contextDebug = glDebug;
  aGraphicDriver->ChangeOptions().contextSyncDebug = glDebug;
  return aGraphicDriver;
}
} // namespace

OcctRenderClient::OcctRenderClient(GLFWwindow *aGlfwWindow) {
  internal_ = std::make_unique<ViewInternal>();
  internal_->glfwWindow = aGlfwWindow;

  if (!internal_->glfwWindow) {
    Message::DefaultMessenger()->Send(
        "OcctRenderClient: GLFW window is null on construction.", Message_Fail);
    return;
  }

  // so we can get the OcctRenderClient pointer from the GLFW window
  glfwSetWindowUserPointer(internal_->glfwWindow, this);

  // Set GLFW callbacks
  glfwSetWindowSizeCallback(internal_->glfwWindow,
                            OcctRenderClient::onResizeCallback);
  glfwSetFramebufferSizeCallback(internal_->glfwWindow,
                                 OcctRenderClient::onFBResizeCallback);
  glfwSetScrollCallback(internal_->glfwWindow,
                        OcctRenderClient::onMouseScrollCallback);
  glfwSetMouseButtonCallback(internal_->glfwWindow,
                             OcctRenderClient::onMouseButtonCallback);
  glfwSetCursorPosCallback(internal_->glfwWindow,
                           OcctRenderClient::onMouseMoveCallback);

  // Create the OCCT Aspect_Window wrapper
#if defined(_WIN32)
  internal_->occtAspectWindow = new WNT_Window(
      (Aspect_Drawable)glfwGetWin32Window(internal_->glfwWindow));
#elif defined(__APPLE__)
  internal_->occtAspectWindow = new Cocoa_Window(
      (Aspect_Drawable)glfwGetCocoaWindow(internal_->glfwWindow));
#else // Linux/X11
  Handle(Aspect_DisplayConnection) aDispConn =
      new Aspect_DisplayConnection((Aspect_XDisplay *)glfwGetX11Display());
  internal_->occtAspectWindow = new Xw_Window(
      aDispConn, (Aspect_Drawable)glfwGetX11Window(internal_->glfwWindow));
#endif
  if (!internal_->occtAspectWindow.IsNull()) {
    internal_->occtAspectWindow->SetVirtual(Standard_True); // Set as virtual
  } else {
    spdlog::error("OcctRenderClient: Failed to create OCCT Aspect_Window wrapper.");
  }
}

OcctRenderClient::~OcctRenderClient() {
  // std::unique_ptr<ViewInternal> internal_ will be automatically destroyed.
}

// Static callback implementations (were previously in .h or implied)
void OcctRenderClient::onResizeCallback(GLFWwindow *theWin, int theWidth,
                                    int theHeight) {
  toView(theWin)->onResize(theWidth, theHeight);
}
void OcctRenderClient::onFBResizeCallback(GLFWwindow *theWin, int theWidth,
                                      int theHeight) {
  toView(theWin)->onResize(theWidth, theHeight);
}
void OcctRenderClient::onMouseScrollCallback(GLFWwindow *theWin,
                                         double referidoOffsetX,
                                         double theOffsetY) {
  toView(theWin)->onMouseScroll(referidoOffsetX, theOffsetY);
}
void OcctRenderClient::onMouseButtonCallback(GLFWwindow *theWin, int theButton,
                                         int theAction, int theMods) {
  toView(theWin)->onMouseButton(theButton, theAction, theMods);
}
void OcctRenderClient::onMouseMoveCallback(GLFWwindow *theWin, double thePosX,
                                       double thePosY) {
  toView(theWin)->onMouseMove((int)thePosX, (int)thePosY);
}

void OcctRenderClient::onContentScaleCallback(GLFWwindow *theWin, float xscale, float yscale) {
  toView(theWin)->onContentScale(xscale, yscale);
}

OcctRenderClient *OcctRenderClient::toView(GLFWwindow *theWin) {
  return static_cast<OcctRenderClient *>(glfwGetWindowUserPointer(theWin));
}

void OcctRenderClient::errorCallback(int theError, const char *theDescription) {
  Message::DefaultMessenger()->Send(TCollection_AsciiString("Error") +
                                        theError + ": " + theDescription,
                                    Message_Fail);
}

void OcctRenderClient::run() {
  if (!internal_->glfwWindow || internal_->occtAspectWindow.IsNull()) {
    spdlog::error("OcctRenderClient::run(): Window not properly initialized.");
    return;
  }

  // Ensure OpenGL context is current for all initialization
  glfwMakeContextCurrent(internal_->glfwWindow);

  try {
    initOCCTRenderingSystem();
    if (internal_->view.IsNull()) {
      spdlog::error("OcctRenderClient::run(): OCCT view initialization failed");
      return;
    }
    
    try {
      initGui(); // Initialize GUI before gRPC to ensure OpenGL context is stable
      spdlog::info("OcctRenderClient::run(): GUI initialization completed");
    } catch (const std::exception& e) {
      spdlog::error("OcctRenderClient::run(): GUI initialization failed: {}", e.what());
      throw;
    }
    
    // Initialize gRPC client with context isolation
    spdlog::info("OcctRenderClient: Initializing gRPC geometry client with OpenGL context isolation...");
    if (initGeometryClient()) {
      spdlog::info("OcctRenderClient::run(): gRPC client initialization completed");
    } else {
      spdlog::warn("OcctRenderClient::run(): gRPC client not available, continuing in standalone mode");
    }
    
    try {
      initDemoScene();
      spdlog::info("OcctRenderClient::run(): Demo scene initialization completed");
    } catch (const std::exception& e) {
      spdlog::error("OcctRenderClient::run(): Demo scene initialization failed: {}", e.what());
      throw;
    }

    try {
      mainloop();
    } catch (const std::exception& e) {
      spdlog::error("OcctRenderClient::run(): Main loop exception: {}", e.what());
      throw;
    }
  } catch (const std::exception& e) {
    spdlog::error("OcctRenderClient::run(): Exception during runtime: {}", e.what());
  } catch (...) {
    spdlog::error("OcctRenderClient::run(): Unknown exception during runtime");
  }
  
  cleanup();
}

void OcctRenderClient::addAisObject(const Handle(AIS_InteractiveObject) &
                                theAisObject) {
  if (theAisObject.IsNull()) {
    spdlog::error("OcctRenderClient::addAisObject(): AIS object is null.");
    return;
  }

  if (internal_->context.IsNull()) {
    spdlog::error("OcctRenderClient::addAisObject(): Context is not initialized.");
    return;
  }

  theAisObject->SetAttributes(getDefaultAISDrawer());
  
  // Get AIS object count before adding
  AIS_ListOfInteractive before_list;
  internal_->context->DisplayedObjects(before_list);
  int before_count = before_list.Size();
  
  // Display the object and trigger view update
  internal_->context->Display(theAisObject, AIS_Shaded, 0, true);
  
  // Get AIS object count after adding
  AIS_ListOfInteractive after_list;
  internal_->context->DisplayedObjects(after_list);
  int after_count = after_list.Size();
  
  // Ensure view is properly updated
  if (!internal_->view.IsNull()) {
    internal_->view->Invalidate();
    internal_->view->Update();
    // Try to fit all objects in view to ensure visibility
    internal_->view->FitAll();
    
    // Log camera position and view state after FitAll
    Handle(Graphic3d_Camera) camera = internal_->view->Camera();
    if (!camera.IsNull()) {
      gp_Pnt eye = camera->Eye();
      gp_Pnt center = camera->Center();
      gp_Dir up = camera->Up();
      spdlog::debug("  Camera Eye: ({:.3f}, {:.3f}, {:.3f})", eye.X(), eye.Y(), eye.Z());
      spdlog::debug("  Camera Center: ({:.3f}, {:.3f}, {:.3f})", center.X(), center.Y(), center.Z());
      spdlog::debug("  Camera Up: ({:.3f}, {:.3f}, {:.3f})", up.X(), up.Y(), up.Z());
      spdlog::debug("  Camera Distance: {:.3f}", camera->Distance());
    }
    
    spdlog::debug("OcctRenderClient::addAisObject(): AIS Context objects: {} -> {}, View updated and fitted", before_count, after_count);
  }
}

void OcctRenderClient::initOCCTRenderingSystem() {
  if (!internal_->glfwWindow || internal_->occtAspectWindow.IsNull()) {
    spdlog::error("No GLFW window or OCCT Aspect_Window found.");
    return;
  }

  initV3dViewer();
  initAisContext();
  initOffscreenRendering();
  initVisualSettings();
}

void OcctRenderClient::initOffscreenRendering() {
  if (internal_->glContext.IsNull() || internal_->view.IsNull()) {
    spdlog::warn(
        "OcctRenderClient::initOffscreenRendering(): GLContext or View is Null.");
    return;
  }

  glfwGetFramebufferSize(internal_->glfwWindow, &internal_->renderWidth,
                         &internal_->renderHeight);

  internal_->offscreenFBO =
      Handle(OpenGl_FrameBuffer)::DownCast(internal_->view->View()->FBOCreate(
          internal_->renderWidth, internal_->renderHeight));
  if (!internal_->offscreenFBO.IsNull()) {
    internal_->offscreenFBO->ColorTexture()->Sampler()->Parameters()->SetFilter(
        Graphic3d_TOTF_BILINEAR);
    internal_->view->View()->SetFBO(internal_->offscreenFBO);
  } else {
    spdlog::error("OcctRenderClient: Failed to create offscreen FBO.");
  }
}

bool OcctRenderClient::resizeOffscreenFramebuffer(int theWidth, int theHeight) {
  assert(internal_->offscreenFBO && internal_->glContext);

  if (internal_->needToResizeFBO ||
      internal_->offscreenFBO->GetSizeX() != theWidth ||
      internal_->offscreenFBO->GetSizeY() != theHeight) {
    internal_->offscreenFBO->InitLazy(internal_->glContext,
                                      Graphic3d_Vec2i(theWidth, theHeight),
                                      GL_RGB8, GL_DEPTH24_STENCIL8);
    internal_->needToResizeFBO = false;
    return true;
  }
  return false;
}

void OcctRenderClient::initGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  // Get DPI scale factor from GLFW
  float xscale, yscale;
  glfwGetWindowContentScale(internal_->glfwWindow, &xscale, &yscale);
  internal_->currentDpiScale = xscale; // Use x-axis scale, typically both are the same
  internal_->lastDpiScale = internal_->currentDpiScale;
  
  spdlog::info("Detected DPI scale factor: {:.2f}", internal_->currentDpiScale);

  ImGui_ImplGlfw_InitForOpenGL(internal_->glfwWindow, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  // Configure ImGui for DPI scaling
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
  
  // Apply DPI scaling
  if (internal_->currentDpiScale > 1.0f) {
    // Scale fonts and UI elements
    io.FontGlobalScale = internal_->currentDpiScale;
    
    // Scale UI spacing and sizes
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(internal_->currentDpiScale);
    
    spdlog::info("Applied DPI scaling: FontGlobalScale={:.2f}, StyleScale={:.2f}", 
                 io.FontGlobalScale, internal_->currentDpiScale);
  } else {
    spdlog::info("No DPI scaling needed (scale factor: {:.2f})", internal_->currentDpiScale);
  }
  
  // Set up content scale callback for dynamic DPI changes
  glfwSetWindowContentScaleCallback(internal_->glfwWindow, onContentScaleCallback);
}

void OcctRenderClient::renderGui() {
  ImGuiIO &aIO = ImGui::GetIO();

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();

  double cursorX, cursorY;
  glfwGetCursorPos(internal_->glfwWindow, &cursorX, &cursorY);
  Graphic3d_Vec2i aMousePos((int)cursorX, (int)cursorY);
  Graphic3d_Vec2i aAdjustedMousePos =
      adjustMousePosition(aMousePos.x(), aMousePos.y());

  // DockSpace - based on GitHub issue #5209 solution
  static ImGuiDockNodeFlags dockspace_flags =
      ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode |
      ImGuiDockNodeFlags_NoDockingOverCentralNode;
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |=
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render
  // our background and handle the pass-thru hole, so we ask Begin() to not
  // render a background.
  if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    window_flags |= ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("DockSpace Demo", nullptr, window_flags);
  ImGui::PopStyleVar();
  ImGui::PopStyleVar(2);

  // Submit the DockSpace
  ImGuiIO &io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    static bool first_time = true;
    if (first_time) {
      first_time = false;

      ImGui::DockBuilderRemoveNode(dockspace_id);
      ImGui::DockBuilderAddNode(dockspace_id,
                                dockspace_flags | ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

      // Use Right split and invert parameters to make OCCT Viewport the central
      // node This ensures proper auto-expanding behavior when side panels are
      // removed
      ImGuiID viewportId = -1; // This will become the central node
      auto renderInfoId = ImGui::DockBuilderSplitNode(
          dockspace_id, ImGuiDir_Right, 0.25f, nullptr, &viewportId);

      // Split render info area for OCAF Demo
      auto ocafDemoId = ImGui::DockBuilderSplitNode(
          renderInfoId, ImGuiDir_Down, 0.5f, nullptr, &renderInfoId);

      // Dock windows to their respective nodes
      ImGui::DockBuilderDockWindow("Render Info", renderInfoId);
      ImGui::DockBuilderDockWindow("OCAF Demo", ocafDemoId);
      ImGui::DockBuilderDockWindow("OCCT Viewport", viewportId);

      // Set central node properties to prevent docking over it
      ImGuiDockNode *central_node = ImGui::DockBuilderGetNode(viewportId);
      if (central_node) {
        central_node->LocalFlags |=
            ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe |
            ImGuiDockNodeFlags_NoDockingOverCentralNode |
            ImGuiDockNodeFlags_NoDockingOverEmpty;
      }

      ImGui::DockBuilderFinish(dockspace_id);
    }
  }

  ImGui::End();

  // Render Info Window - now dockable
  if (ImGui::Begin("Render Info")) {
    // Display and DPI information
    ImGui::SeparatorText("Display Information");
    ImGui::Text("DPI Scale Factor: %.2f", internal_->currentDpiScale);
    ImGui::Text("Font Scale: %.2f", ImGui::GetIO().FontGlobalScale);
    
    // Get monitor information
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    if (primaryMonitor) {
      const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
      if (mode) {
        ImGui::Text("Monitor Resolution: %d x %d", mode->width, mode->height);
      }
    }
    
    int width, height;
    glfwGetWindowSize(internal_->glfwWindow, &width, &height);
    ImGui::Text("Window Size: %d x %d", width, height);
    ImGui::Text("Render Size: %d x %d", internal_->renderWidth,
                internal_->renderHeight);
    ImGui::Text("Need Resize FBO: %s",
                internal_->needToResizeFBO ? "Yes" : "No");
    ImGui::Text("Viewport Size: %.1f x %.1f", internal_->viewport.x,
                internal_->viewport.y);
    ImGui::Text("View Position: %.1f, %.1f", internal_->viewPos.x,
                internal_->viewPos.y);
    ImGui::Text("Mouse Over Render Area: %s",
                internal_->renderWindowHasFocus ? "Yes" : "No");

    ImGui::SeparatorText("Mouse Position");
    ImGui::Text("Mouse Position (Original): %d, %d", aMousePos.x(),
                aMousePos.y());
    ImGui::Text("Mouse Position (Adjusted): %d, %d", aAdjustedMousePos.x(),
                aAdjustedMousePos.y());
    if (internal_->renderWindowHasFocus) {
      ImGui::Text("Mouse Offset: %d, %d", aMousePos.x() - aAdjustedMousePos.x(),
                  aMousePos.y() - aAdjustedMousePos.y());
      ImGui::Text("Position in View: %.3f, %.3f, %.3f",
                  internal_->positionInView.X(), internal_->positionInView.Y(),
                  internal_->positionInView.Z());
    }

    if (internal_->renderWindowHasFocus) {
      ImGui::SeparatorText("Mouse States");
      ImGui::Text("Last Mouse Position: %d, %d", LastMousePosition().x(),
                  LastMousePosition().y());
      Aspect_VKeyMouse aButtons = PressedMouseButtons();
      ImGui::Text("Pressed Mouse Buttons: 0x%X", aButtons);
      ImGui::Text("- Left: %s",
                  (aButtons & Aspect_VKeyMouse_LeftButton) ? "Yes" : "No");
      ImGui::Text("- Right: %s",
                  (aButtons & Aspect_VKeyMouse_RightButton) ? "Yes" : "No");
      ImGui::Text("- Middle: %s",
                  (aButtons & Aspect_VKeyMouse_MiddleButton) ? "Yes" : "No");
      Aspect_VKeyFlags aFlags = LastMouseFlags();
      ImGui::Text("Last Mouse Flags: 0x%X", aFlags);
      ImGui::Text("- Shift: %s",
                  (aFlags & Aspect_VKeyFlags_SHIFT) ? "Yes" : "No");
      ImGui::Text("- Ctrl: %s",
                  (aFlags & Aspect_VKeyFlags_CTRL) ? "Yes" : "No");
      ImGui::Text("- Alt: %s", (aFlags & Aspect_VKeyFlags_ALT) ? "Yes" : "No");
      ImGui::Text("- Meta: %s",
                  (aFlags & Aspect_VKeyFlags_META) ? "Yes" : "No");
    }

    ImGui::Text("Events Time Info:");
    ImGui::Text("- Last Event Time: %.2f", EventTime());

    ImGui::Separator();
    ImGui::Text("ToAskNextFrame: %s",
                myToAskNextFrame
                    ? "Yes"
                    : "No"); // myToAskNextFrame is from AIS_ViewController

    ImGui::Separator();
    ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);

    ImGui::SeparatorText("View Cube");
    if (ImGui::Checkbox("Fixed View Cube Animation Loop",
                        &internal_->fixedViewCubeAnimationLoop)) {
      if (internal_->fixedViewCubeAnimationLoop) {
        internal_->viewCube->SetDuration(0.1);
        internal_->viewCube->SetFixedAnimationLoop(true);
      } else {
        internal_->viewCube->SetDuration(0.5);
        internal_->viewCube->SetFixedAnimationLoop(false);
      }
    }

    ImGui::SeparatorText("Selection");
    static bool vertexSelection = false;
    static bool edgeSelection = false;
    static bool faceSelection = false;
    static bool compoundSelection = false;
    bool changed = ImGui::Checkbox("Vertex", &vertexSelection);
    changed |= ImGui::Checkbox("Edge", &edgeSelection);
    changed |= ImGui::Checkbox("Face", &faceSelection);
    changed |= ImGui::Checkbox("Compound", &compoundSelection);

    if (changed) {
      internal_->context->Deactivate();
      // FIXME: activate the selection mode for each displayed object
      if (vertexSelection) {
        internal_->context->Activate(AIS_Shape::SelectionMode(TopAbs_VERTEX));
      }
      if (edgeSelection) {
        internal_->context->Activate(AIS_Shape::SelectionMode(TopAbs_EDGE));
      }
      if (faceSelection) {
        internal_->context->Activate(AIS_Shape::SelectionMode(TopAbs_FACE));
      }
      if (compoundSelection) {
        internal_->context->Activate(AIS_Shape::SelectionMode(TopAbs_COMPOUND));
      }
    }
    // Activate the view cube to allow it to be normally selected
    // FIXME: if we activate the selection mode for each object except the view
    // cube, we don't need to activate the view cube here
    internal_->context->Activate(internal_->viewCube);
  }
  ImGui::End();

  // gRPC Control Panel Window
  if (internal_->showGrpcControlPanel) {
    if (ImGui::Begin("gRPC Control Panel", &internal_->showGrpcControlPanel)) {
      // Connection status display
      ImGui::Text("Server: localhost:50051");
    
      // Check actual connection status
      bool isConnected = internal_->geometryClient && internal_->geometryClient->IsConnected();
      
      // Update internal connection status based on actual state
      if (isConnected && internal_->connectionStatus != ViewInternal::ConnectionStatus::Connected) {
        internal_->connectionStatus = ViewInternal::ConnectionStatus::Connected;
      } else if (!isConnected && internal_->connectionStatus == ViewInternal::ConnectionStatus::Connected) {
        internal_->connectionStatus = ViewInternal::ConnectionStatus::Disconnected;
        internal_->connectionErrorMessage = "Connection lost";
      }
      
      // Display connection status with color coding
      const char* statusText = "Unknown";
      ImVec4 statusColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Gray
      
      switch (internal_->connectionStatus) {
        case ViewInternal::ConnectionStatus::Connected:
          statusText = "Connected";
          statusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
          break;
        case ViewInternal::ConnectionStatus::Connecting:
          statusText = "Connecting...";
          statusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
          break;
        case ViewInternal::ConnectionStatus::Disconnected:
          statusText = "Disconnected";
          statusColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f); // Orange
          break;
        case ViewInternal::ConnectionStatus::Error:
          statusText = "Error";
          statusColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
          break;
      }
      
      ImGui::TextColored(statusColor, "Status: %s", statusText);
      
      // Show error message if any
      if (!internal_->connectionErrorMessage.empty() && internal_->connectionStatus != ViewInternal::ConnectionStatus::Connected) {
        ImGui::TextWrapped("Error: %s", internal_->connectionErrorMessage.c_str());
      }
      
      // Connection management buttons
      if (!isConnected) {
        // Auto-reconnect logic
        float current_time = static_cast<float>(glfwGetTime());
        float time_since_last_attempt = current_time - internal_->lastConnectionAttemptTime;
        
        // Show manual reconnect button or auto-reconnect status
        if (ImGui::Button("Connect to Server")) {
          spdlog::info("gRPC Control Panel: User requested manual connection");
          startAsyncConnection();
        }
        
        // Show time until next auto-reconnect attempt
        if (internal_->connectionStatus == ViewInternal::ConnectionStatus::Disconnected || 
            internal_->connectionStatus == ViewInternal::ConnectionStatus::Error) {
          float time_until_retry = internal_->RECONNECT_INTERVAL - time_since_last_attempt;
          if (time_until_retry > 0) {
            ImGui::TextDisabled("Auto-reconnect in %.1fs", time_until_retry);
          } else {
            // Attempt auto-reconnect
            ImGui::TextDisabled("Attempting auto-reconnect...");
            startAsyncConnection();
          }
        }
      } else {
        if (ImGui::Button("Disconnect")) {
          spdlog::info("gRPC Control Panel: User requested disconnection");
          shutdownGeometryClient();
          internal_->connectionStatus = ViewInternal::ConnectionStatus::Disconnected;
        }
      }
      
      ImGui::Separator();
      
      if (isConnected) {
        // Geometry operations
        ImGui::Text("Geometry Operations");
        
        if (ImGui::Button("Create Random Box")) {
          spdlog::info("gRPC Control Panel: User clicked Create Random Box button");
          try {
            createRandomBox();
            spdlog::info("gRPC Control Panel: Create Random Box completed successfully");
          } catch (const std::exception& e) {
            spdlog::error("gRPC Control Panel: Create Random Box failed: {}", e.what());
          }
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Random Cone")) {
          spdlog::info("gRPC Control Panel: User clicked Create Random Cone button");
          try {
            createRandomCone();
            spdlog::info("gRPC Control Panel: Create Random Cone completed successfully");
          } catch (const std::exception& e) {
            spdlog::error("gRPC Control Panel: Create Random Cone failed: {}", e.what());
          }
        }
        
        if (ImGui::Button("Create Demo Scene")) {
          spdlog::info("gRPC Control Panel: User clicked Create Demo Scene button");
          try {
            createDemoScene();
            spdlog::info("gRPC Control Panel: Create Demo Scene completed successfully");
          } catch (const std::exception& e) {
            spdlog::error("gRPC Control Panel: Create Demo Scene failed: {}", e.what());
          }
        }
        
        if (ImGui::Button("Clear All Shapes")) {
          spdlog::info("gRPC Control Panel: User clicked Clear All Shapes button");
          try {
            clearAllShapes();
            spdlog::info("gRPC Control Panel: Clear All Shapes completed successfully");
          } catch (const std::exception& e) {
            spdlog::error("gRPC Control Panel: Clear All Shapes failed: {}", e.what());
          }
        }
        
        ImGui::Separator();
        
        // Mesh operations
        ImGui::Text("Mesh Operations");
        
        if (ImGui::Button("Refresh Meshes")) {
          spdlog::info("gRPC Control Panel: User clicked Refresh Meshes button");
          try {
            refreshMeshes();
            spdlog::info("gRPC Control Panel: Refresh Meshes completed successfully");
          } catch (const std::exception& e) {
            spdlog::error("gRPC Control Panel: Refresh Meshes failed: {}", e.what());
          }
        }
        
        if (ImGui::Checkbox("Auto Refresh", &internal_->autoRefreshMeshes)) {
          spdlog::info("gRPC Control Panel: Auto Refresh toggled to: {}", internal_->autoRefreshMeshes ? "ON" : "OFF");
        }
        
        ImGui::Separator();
        
        // Server statistics with caching to avoid excessive network calls
        float current_time = static_cast<float>(glfwGetTime());
        bool should_update = !internal_->hasValidSystemInfo || 
                            (current_time - internal_->lastSystemInfoUpdateTime) >= internal_->SYSTEM_INFO_UPDATE_INTERVAL;
        
        if (should_update && internal_->geometryClient && internal_->geometryClient->IsConnected()) {
          try {
            internal_->cachedSystemInfo = internal_->geometryClient->GetSystemInfo();
            internal_->lastSystemInfoUpdateTime = current_time;
            internal_->hasValidSystemInfo = true;
          } catch (const std::exception& e) {
            spdlog::debug("gRPC Control Panel: Error updating server statistics: {}", e.what());
            internal_->hasValidSystemInfo = false;
          } catch (...) {
            spdlog::debug("gRPC Control Panel: Unknown error updating server statistics");
            internal_->hasValidSystemInfo = false;
          }
        }
        
        // Display cached system info
        ImGui::Text("Server Statistics");
        if (internal_->hasValidSystemInfo) {
          ImGui::Text("Active Shapes: %d", internal_->cachedSystemInfo.active_shapes);
          ImGui::Text("Server Version: %s", internal_->cachedSystemInfo.version.c_str());
          ImGui::Text("OCCT Version: %s", internal_->cachedSystemInfo.occt_version.c_str());
          
          // Show update indicator
          float time_since_update = current_time - internal_->lastSystemInfoUpdateTime;
          ImGui::TextDisabled("(Updated %.1fs ago)", time_since_update);
        } else {
          ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Server Statistics: Not available");
        }
        
        ImGui::Separator();
        
        // Performance monitoring controls
        ImGui::Text("Performance Monitoring");
        if (ImGui::Checkbox("Show Performance Panel", &internal_->showGrpcPerformancePanel)) {
          spdlog::info("gRPC Control Panel: Performance Panel toggled to: {}", internal_->showGrpcPerformancePanel ? "ON" : "OFF");
        }
      }
    }
    ImGui::End();
  }

  // gRPC Performance Panel Window
  if (internal_->showGrpcPerformancePanel && internal_->performancePanel) {
    internal_->performancePanel->setVisible(true);
    internal_->performancePanel->render();
  } else if (internal_->performancePanel) {
    internal_->performancePanel->setVisible(false);
  }

  // OCCT Viewport Window - ensure it stays docked
  ImGuiWindowFlags viewport_window_flags = ImGuiWindowFlags_NoCollapse;

  // Prevent the viewport from floating by forcing it back to dock
  static bool viewport_should_dock = true;
  if (viewport_should_dock) {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGuiDockNode *central_node =
        ImGui::DockBuilderGetCentralNode(dockspace_id);
    if (central_node) {
      ImGui::SetNextWindowDockID(central_node->ID, ImGuiCond_Appearing);
    }
  }

  if (ImGui::Begin("OCCT Viewport", nullptr, viewport_window_flags)) {
    // Ensure central node protection is maintained
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGuiDockNode *central_node =
        ImGui::DockBuilderGetCentralNode(dockspace_id);
    if (central_node) {
      // Maintain protection flags
      central_node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar |
                                  ImGuiDockNodeFlags_NoDockingOverMe |
                                  ImGuiDockNodeFlags_NoDockingOverCentralNode |
                                  ImGuiDockNodeFlags_NoDockingOverEmpty;
    }

    // Check if window is floating and force it back to dock
    if (ImGui::IsWindowDocked() == false && central_node) {
      ImGui::SetNextWindowDockID(central_node->ID, ImGuiCond_Always);
    }
    ImVec2 newViewportSize = ImGui::GetContentRegionAvail();
    int newRenderWidth = static_cast<int>(newViewportSize.x);
    int newRenderHeight = static_cast<int>(newViewportSize.y);

    // Check if the render dimensions need to change
    if (internal_->renderWidth != newRenderWidth ||
        internal_->renderHeight != newRenderHeight) {
      internal_->renderWidth = newRenderWidth;
      internal_->renderHeight = newRenderHeight;
      internal_->needToResizeFBO =
          true; // Signal that the FBO should be checked/resized
      internal_->viewport =
          newViewportSize; // Update stored ImGui viewport size

      // Update OCCT's window representation (Aspect_Window)
      if (internal_->occtAspectWindow) {
#if defined(_WIN32)
        Handle(WNT_Window)::DownCast(internal_->occtAspectWindow)
            ->SetPos(0, 0, newRenderWidth, newRenderHeight);
#elif defined(__APPLE__)
        // Note: Cocoa_Window might not have a direct SetSize or equivalent.
        // Behavior might differ on macOS.
        spdlog::debug("OCCT View: Cocoa_Window size update may require "
                      "platform-specific handling for SetSize.");
#else // Linux/X11
        Handle(Xw_Window)::DownCast(internal_->occtAspectWindow)
            ->SetSize(newRenderWidth, newRenderHeight);
#endif
        // If the underlying OCCT window resizes, the V3d_View also needs to be
        // notified.
        if (internal_->view) {
          internal_->view->MustBeResized();
        }
      }

      // Update V3d_View's camera aspect ratio
      if (internal_->view) {
        internal_->view->Camera()->SetAspect(
            static_cast<float>(newRenderWidth) /
            static_cast<float>(newRenderHeight));
      }
    }

    // Attempt to resize the offscreen framebuffer.
    // This uses internal_->renderWidth and internal_->renderHeight, which are
    // now updated if there was a change. It also considers the
    // internal_->needToResizeFBO flag.
    if (resizeOffscreenFramebuffer(internal_->renderWidth,
                                   internal_->renderHeight)) {
      // If the FBO was actually resized, the view needs to be redrawn.
      if (internal_->view) {
        internal_->view->Redraw();
      }
    }

    internal_->viewPos = ImGui::GetCursorScreenPos();

    if (!internal_->offscreenFBO.IsNull() &&
        internal_->offscreenFBO->ColorTexture()->TextureId() != 0) {
      ImGui::Image(
          (ImTextureID)(uintptr_t)internal_->offscreenFBO->ColorTexture()
              ->TextureId(),
          newViewportSize, ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));

      internal_->renderWindowHasFocus = ImGui::IsItemHovered();
    }
  }
  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  // Update and Render additional Platform Windows
  // (Platform functions may change the current OpenGL context, so we
  // save/restore it to make it easier to paste this code elsewhere.
  //  For this specific demo app we could also call
  //  glfwMakeContextCurrent(window) directly)
  if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    GLFWwindow *backup_current_context = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup_current_context);
  }

  glfwSwapBuffers(internal_->glfwWindow);
}

void OcctRenderClient::initDemoScene() {
  if (internal_->context.IsNull()) {
    return;
  }

  internal_->view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_GOLD,
                                   0.08, V3d_WIREFRAME);

  // Try to load demo scene from gRPC server first
  if (internal_->geometryClient && internal_->geometryClient->IsConnected()) {
    spdlog::info("OcctRenderClient: Loading demo scene from gRPC server...");
    try {
      createDemoScene(); // This will call gRPC server to create demo scene
      spdlog::info("OcctRenderClient: gRPC demo scene loaded, also creating local shapes for comparison");
      // Don't return here - continue to create local shapes as well
    } catch (const std::exception& e) {
      spdlog::warn("OcctRenderClient: Failed to load gRPC demo scene, falling back to local shapes: {}", e.what());
    }
  }

  // No local OCCT shapes creation - only show server shapes, grid and view cube
  spdlog::info("OcctRenderClient: Skipping local shape creation - showing only server shapes, grid and view cube");

  TCollection_AsciiString aGlInfo;
  {
    TColStd_IndexedDataMapOfStringString aRendInfo;
    internal_->view->DiagnosticInformation(aRendInfo,
                                           Graphic3d_DiagnosticInfo_Basic);
    for (TColStd_IndexedDataMapOfStringString::Iterator aValueIter(aRendInfo);
         aValueIter.More(); aValueIter.Next()) {
      if (!aGlInfo.IsEmpty()) {
        aGlInfo += "\n";
      }
      aGlInfo += TCollection_AsciiString("  ") + aValueIter.Key() + ": " +
                 aValueIter.Value();
    }
  }
  spdlog::info("OpenGL info: \n{}", aGlInfo.ToCString());
}

void OcctRenderClient::initV3dViewer() {
  spdlog::info("Initializing OCCT 3D Viewer.");

  assert(internal_->occtAspectWindow.IsNull() == false);

  Handle(Aspect_DisplayConnection) aDispConn;
#if !defined(_WIN32) && !defined(__APPLE__) // For X11
  aDispConn = internal_->occtAspectWindow->GetDisplay();
#else
  aDispConn = new Aspect_DisplayConnection();
#endif
  Handle(OpenGl_GraphicDriver) aGraphicDriver = createOpenGlDriver(aDispConn);

  auto &viewer_ = internal_->viewer; // shortcut
  auto &view_ = internal_->view;     // shortcut

  viewer_ = new V3d_Viewer(aGraphicDriver);
  viewer_->SetLightOn(
      new V3d_DirectionalLight(V3d_Zneg, Quantity_NOC_WHITE, true));
  viewer_->SetLightOn(new V3d_AmbientLight(Quantity_NOC_WHITE));
  viewer_->SetDefaultTypeOfView(V3d_ORTHOGRAPHIC);
  viewer_->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
  view_ = viewer_->CreateView();
  view_->SetImmediateUpdate(Standard_False);

  Aspect_RenderingContext aNativeGlContext = NULL;
#if defined(_WIN32)
  aNativeGlContext = glfwGetWGLContext(internal_->glfwWindow);
#elif defined(__APPLE__)
  aNativeGlContext =
      (Aspect_RenderingContext)glfwGetNSGLContext(internal_->glfwWindow);
#else // Linux/X11
  aNativeGlContext = glfwGetGLXContext(internal_->glfwWindow);
#endif
  view_->SetWindow(internal_->occtAspectWindow, aNativeGlContext);

  internal_->view->ChangeRenderingParams().ToShowStats = Standard_True;
  internal_->view->ChangeRenderingParams().CollectedStats =
      Graphic3d_RenderingParams::PerfCounters_All;

  internal_->glContext = aGraphicDriver->GetSharedContext();
  if (internal_->glContext.IsNull()) {
    spdlog::error("OcctRenderClient: Failed to get OpenGl_Context.");
    assert(false);
  }
}

void OcctRenderClient::initAisContext() {
  spdlog::info("Initializing OCCT AIS Context.");

  assert(internal_->viewer.IsNull() == false);
  auto &context_ = internal_->context; // shortcut

  context_ = new AIS_InteractiveContext(internal_->viewer);
  context_->SetAutoActivateSelection(true);
  context_->SetToHilightSelected(false);
  context_->SetPickingStrategy(SelectMgr_PickingStrategy_OnlyTopmost);
  context_->SetDisplayMode(AIS_Shaded, false);
  context_->EnableDrawHiddenLine();
  context_->SetPixelTolerance(2.0);

  auto &default_drawer = context_->DefaultDrawer();
  default_drawer->SetWireAspect(
      new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.0));
  default_drawer->SetTypeOfHLR(Prs3d_TOH_PolyAlgo);

  constexpr bool s_display_viewcube = true;

  if constexpr (s_display_viewcube) {
    internal_->viewCube = new AIS_ViewCube();
    internal_->viewCube->SetSize(55);
    internal_->viewCube->SetFontHeight(12);
    internal_->viewCube->SetAxesLabels("", "", "");
    internal_->viewCube->SetTransformPersistence(new Graphic3d_TransformPers(
        Graphic3d_TMF_TriedronPers, Aspect_TOTP_LEFT_LOWER,
        Graphic3d_Vec2i(100, 100)));
    if (this->ViewAnimation()) {
      internal_->viewCube->SetViewAnimation(this->ViewAnimation());
    } else {
      spdlog::warn("OcctRenderClient: ViewAnimation not available for ViewCube.");
    }

    if (internal_->fixedViewCubeAnimationLoop) {
      internal_->viewCube->SetDuration(0.1);
      internal_->viewCube->SetFixedAnimationLoop(true);
    } else {
      internal_->viewCube->SetDuration(0.5);
      internal_->viewCube->SetFixedAnimationLoop(false);
    }
    internal_->context->Display(internal_->viewCube, false);
  }
}

void OcctRenderClient::initVisualSettings() {
  spdlog::info("Initializing OCCT Visual Settings.");

  assert(internal_->context.IsNull() == false);
  auto &context_ = internal_->context; // shortcut

  // Higlight Selected
  Handle(Prs3d_Drawer) selectionDrawer = new Prs3d_Drawer();
  selectionDrawer->SetupOwnDefaults();
  selectionDrawer->SetColor(internal_->selectionColor);
  selectionDrawer->SetDisplayMode(0);
  selectionDrawer->SetZLayer(Graphic3d_ZLayerId_Default);
  selectionDrawer->SetTypeOfDeflection(Aspect_TOD_RELATIVE);
  selectionDrawer->SetDeviationAngle(context_->DeviationAngle());
  selectionDrawer->SetDeviationCoefficient(context_->DeviationCoefficient());
  context_->SetSelectionStyle(
      selectionDrawer); // equal to
                        // SetHighlightStyle(Prs3d_TypeOfHighlight_Selected,
                        // selectionDrawer);
  context_->SetHighlightStyle(Prs3d_TypeOfHighlight_LocalSelected,
                              selectionDrawer);
  context_->SetHighlightStyle(Prs3d_TypeOfHighlight_SubIntensity,
                              selectionDrawer);

  // Higlight Dynamic
  Handle(Prs3d_Drawer) hilightDrawer = new Prs3d_Drawer();
  hilightDrawer->SetupOwnDefaults();
  hilightDrawer->SetColor(internal_->highlightColor);
  hilightDrawer->SetDisplayMode(0);
  hilightDrawer->SetZLayer(Graphic3d_ZLayerId_Top);
  hilightDrawer->SetTypeOfDeflection(Aspect_TOD_RELATIVE);
  hilightDrawer->SetDeviationAngle(context_->DeviationAngle());
  hilightDrawer->SetDeviationCoefficient(context_->DeviationCoefficient());
  context_->SetHighlightStyle(Prs3d_TypeOfHighlight_Dynamic, hilightDrawer);

  // Higlight Local
  Handle(Prs3d_Drawer) hilightLocalDrawer = new Prs3d_Drawer();
  hilightLocalDrawer->SetupOwnDefaults();
  hilightLocalDrawer->SetColor(internal_->highlightColor);
  hilightLocalDrawer->SetDisplayMode(1);
  hilightLocalDrawer->SetZLayer(Graphic3d_ZLayerId_Top);
  hilightLocalDrawer->SetTypeOfDeflection(Aspect_TOD_RELATIVE);
  hilightLocalDrawer->SetDeviationAngle(context_->DeviationAngle());
  hilightLocalDrawer->SetDeviationCoefficient(context_->DeviationCoefficient());

  Handle(Prs3d_ShadingAspect) shadingAspect = new Prs3d_ShadingAspect();
  shadingAspect->SetColor(internal_->highlightColor);
  shadingAspect->SetTransparency(0);
  shadingAspect->Aspect()->SetPolygonOffsets((int)Aspect_POM_Fill, 0.99f, 0.0f);
  hilightLocalDrawer->SetShadingAspect(shadingAspect);

  Handle(Prs3d_LineAspect) lineAspect =
      new Prs3d_LineAspect(internal_->highlightColor, Aspect_TOL_SOLID, 3.0);
  hilightLocalDrawer->SetLineAspect(lineAspect);
  hilightLocalDrawer->SetSeenLineAspect(lineAspect);
  hilightLocalDrawer->SetWireAspect(lineAspect);
  hilightLocalDrawer->SetFaceBoundaryAspect(lineAspect);
  hilightLocalDrawer->SetFreeBoundaryAspect(lineAspect);
  hilightLocalDrawer->SetUnFreeBoundaryAspect(lineAspect);

  context_->SetHighlightStyle(Prs3d_TypeOfHighlight_LocalDynamic,
                              hilightLocalDrawer);
}

void OcctRenderClient::handleViewRedraw(const Handle(AIS_InteractiveContext)& theCtx,
                                        const Handle(V3d_View)& theView) {
  // Handle view redraw requests from AIS_ViewController
  // This method is called when the view needs to be updated
  if (!theView.IsNull()) {
    theView->Invalidate();
    theView->Update();
    spdlog::debug("OcctRenderClient::handleViewRedraw(): View updated");
  }
}

void OcctRenderClient::mainloop() {
  if (!internal_->glfwWindow)
    return;

  while (!glfwWindowShouldClose(internal_->glfwWindow)) {
    if (!toAskNextFrame()) { // toAskNextFrame is from AIS_ViewController
      glfwWaitEvents();
    } else {
      glfwPollEvents();
    }
    if (!internal_->view.IsNull()) {
      // No need to invalidate the immediate layer each frame
      FlushViewEvents(internal_->context, internal_->view, Standard_True);
      renderGui();
    }
  }
}

void OcctRenderClient::cleanup() {
  spdlog::info("OcctRenderClient::cleanup(): Starting cleanup process with improved OpenGL context management");
  
  try {
    // Step 1: Early gRPC cleanup to prevent context conflicts
    shutdownGeometryClient();

    // Step 2: Ensure we have a valid OpenGL context for all cleanup operations
    bool contextValid = false;
    if (internal_->glfwWindow) {
      glfwMakeContextCurrent(internal_->glfwWindow);
      contextValid = (glfwGetCurrentContext() == internal_->glfwWindow);
      spdlog::info("OcctRenderClient::cleanup(): OpenGL context valid: {}", contextValid);
    }

    if (!contextValid) {
      spdlog::warn("OcctRenderClient::cleanup(): OpenGL context not valid, attempting basic cleanup only");
      return;
    }

    // Step 3: Clean up OCCT objects in proper order while context is valid
    try {
      if (!internal_->context.IsNull()) {
        spdlog::debug("OcctRenderClient::cleanup(): Removing all AIS objects");
        internal_->context->RemoveAll(Standard_False);
        internal_->context.Nullify();
      }
    } catch (...) {
      spdlog::warn("OcctRenderClient::cleanup(): Exception during AIS context cleanup, continuing");
    }

    // Step 4: Release offscreen FBO early while context is guaranteed valid
    try {
      if (!internal_->offscreenFBO.IsNull() && !internal_->glContext.IsNull()) {
        spdlog::debug("OcctRenderClient::cleanup(): Releasing offscreen FBO");
        internal_->offscreenFBO->Release(internal_->glContext.get());
        internal_->offscreenFBO.Nullify();
      }
    } catch (...) {
      spdlog::warn("OcctRenderClient::cleanup(): Exception during FBO cleanup, continuing");
    }

    // Step 5: Clean up OCCT view and viewer
    try {
      if (!internal_->view.IsNull()) {
        spdlog::debug("OcctRenderClient::cleanup(): Removing OCCT view");
        internal_->view->Remove();
        internal_->view.Nullify();
      }
    } catch (...) {
      spdlog::warn("OcctRenderClient::cleanup(): Exception during view cleanup, continuing");
    }

    try {
      if (!internal_->viewer.IsNull()) {
        spdlog::debug("OcctRenderClient::cleanup(): Nullifying OCCT viewer");
        internal_->viewer.Nullify();
      }
    } catch (...) {
      spdlog::warn("OcctRenderClient::cleanup(): Exception during viewer cleanup, continuing");
    }

    // Step 6: Clean up ImGui resources
    try {
      spdlog::debug("OcctRenderClient::cleanup(): Cleaning up ImGui resources");
      if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
      }
    } catch (...) {
      spdlog::warn("OcctRenderClient::cleanup(): Exception during ImGui cleanup, continuing");
    }

    // Step 7: Final cleanup of OCCT window and OpenGL context
    try {
      spdlog::debug("OcctRenderClient::cleanup(): Final cleanup of OCCT window and OpenGL context");
      internal_->occtAspectWindow.Nullify();
      internal_->glContext.Nullify();
    } catch (...) {
      spdlog::warn("OcctRenderClient::cleanup(): Exception during final cleanup, continuing");
    }
    
    spdlog::info("OcctRenderClient::cleanup(): Cleanup completed successfully");
    
  } catch (...) {
    spdlog::warn("OcctRenderClient::cleanup(): Overall exception during cleanup, suppressing for stable shutdown");
  }
}

void OcctRenderClient::onResize(int theWidth, int theHeight) {
  if (theWidth != 0 && theHeight != 0 && !internal_->view.IsNull()) {
    internal_->view->MustBeResized();
    internal_->view->Invalidate(); // invalidate the whole view
  }
}

Graphic3d_Vec2i OcctRenderClient::adjustMousePosition(int thePosX,
                                                  int thePosY) const {
  if (internal_->renderWindowHasFocus) {
    // In multi-viewport mode, we need to convert coordinates properly
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      // Get the current ImGui window's viewport position in screen coordinates
      ImGuiViewport *currentViewport =
          ImGui::FindViewportByPlatformHandle(internal_->glfwWindow);
      if (currentViewport) {
        // Convert GLFW window coordinates to screen coordinates
        int windowPosX, windowPosY;
        glfwGetWindowPos(internal_->glfwWindow, &windowPosX, &windowPosY);
        int screenMouseX = windowPosX + thePosX;
        int screenMouseY = windowPosY + thePosY;

        // Convert to viewport-relative coordinates
        return Graphic3d_Vec2i(
            screenMouseX - static_cast<int>(internal_->viewPos.x),
            screenMouseY - static_cast<int>(internal_->viewPos.y));
      }
    }

    // Fallback for single viewport mode
    return Graphic3d_Vec2i(thePosX - static_cast<int>(internal_->viewPos.x),
                           thePosY - static_cast<int>(internal_->viewPos.y));
  }
  return Graphic3d_Vec2i(thePosX, thePosY);
}

void OcctRenderClient::onMouseButton(int theButton, int theAction, int theMods) {
  if (internal_->view.IsNull() || !internal_->renderWindowHasFocus ||
      !internal_->glfwWindow) {
    return;
  }

  double cursorX, cursorY;
  glfwGetCursorPos(internal_->glfwWindow, &cursorX, &cursorY);
  Graphic3d_Vec2i aAdjustedPos =
      adjustMousePosition((int)cursorX, (int)cursorY);

  if (theAction == GLFW_PRESS) {
    if (PressMouseButton(aAdjustedPos, mouseButtonFromGlfw(theButton),
                         keyFlagsFromGlfw(theMods), false)) {
      internal_->view->InvalidateImmediate();
    }
  } else {
    if (ReleaseMouseButton(aAdjustedPos, mouseButtonFromGlfw(theButton),
                           keyFlagsFromGlfw(theMods), false)) {
      internal_->view->InvalidateImmediate();
    }
  }
}

void OcctRenderClient::onMouseMove(int thePosX, int thePosY) {
  if (internal_->view.IsNull() || !internal_->renderWindowHasFocus) {
    // When we are not in the render window, we need to reset the view input
    // to avoid accumulating mouse movements
    ResetViewInput();
    return;
  }

  Graphic3d_Vec2i aAdjustedPos = adjustMousePosition(thePosX, thePosY);

  // Record the 3D position in the view
  auto selector = internal_->context->MainSelector();
  selector->Pick(aAdjustedPos.x(), aAdjustedPos.y(), internal_->view);
  internal_->positionInView =
      selector->NbPicked() > 0
          ? selector->PickedPoint(1)
          : screenToViewCoordinates(aAdjustedPos.x(), aAdjustedPos.y());

  if (UpdateMousePosition(aAdjustedPos, PressedMouseButtons(), LastMouseFlags(),
                          Standard_False)) {
    internal_->view->InvalidateImmediate();
  }
}

void OcctRenderClient::onMouseScroll(double theOffsetX, double theOffsetY) {
  if (internal_->view.IsNull() || !internal_->renderWindowHasFocus ||
      !internal_->glfwWindow) {
    return;
  }

  double cursorX, cursorY;
  glfwGetCursorPos(internal_->glfwWindow, &cursorX, &cursorY);
  Graphic3d_Vec2i aAdjustedPos = adjustMousePosition(cursorX, cursorY);
  if (UpdateZoom(Aspect_ScrollDelta(aAdjustedPos,
                                    int(theOffsetY * myScrollZoomRatio)))) {
    internal_->view->InvalidateImmediate();
  }
}

void OcctRenderClient::onContentScale(float xscale, float yscale) {
  internal_->currentDpiScale = xscale; // Use x-axis scale
  
  // Only apply scaling if it changed significantly (avoid unnecessary updates)
  float scaleDiff = std::abs(internal_->currentDpiScale - internal_->lastDpiScale);
  if (scaleDiff > 0.01f) { // 1% threshold
    spdlog::info("DPI scale changed: {:.2f} -> {:.2f}", 
                 internal_->lastDpiScale, internal_->currentDpiScale);
    
    // Update ImGui scaling
    ImGuiIO& io = ImGui::GetIO();
    
    // Calculate relative scale factor
    float relativeScale = internal_->currentDpiScale / internal_->lastDpiScale;
    
    // Apply font scaling
    io.FontGlobalScale = internal_->currentDpiScale;
    
    // Apply style scaling (restore original then apply new scale)
    ImGuiStyle& style = ImGui::GetStyle();
    
    // For existing styles, we need to scale relatively
    if (internal_->lastDpiScale > 0.0f) {
      // First restore to original scale, then apply new scale  
      float restoreScale = 1.0f / internal_->lastDpiScale;
      style.ScaleAllSizes(restoreScale);
      style.ScaleAllSizes(internal_->currentDpiScale);
    } else {
      // Fresh scaling
      style.ScaleAllSizes(internal_->currentDpiScale);
    }
    
    internal_->lastDpiScale = internal_->currentDpiScale;
    
    spdlog::info("Applied dynamic DPI scaling: FontScale={:.2f}, StyleScale={:.2f}", 
                 io.FontGlobalScale, internal_->currentDpiScale);
  }
}

// Convert screen coordinates to 3D coordinates in the view
// copy from mayo: graphics_utils.cpp
gp_Pnt OcctRenderClient::screenToViewCoordinates(int theX, int theY) const {
  double xEye, yEye, zEye, xAt, yAt, zAt;
  internal_->view->Eye(xEye, yEye, zEye);
  internal_->view->At(xAt, yAt, zAt);
  const gp_Pnt pntEye(xEye, yEye, zEye);
  const gp_Pnt pntAt(xAt, yAt, zAt);

  const gp_Vec vecEye(pntEye, pntAt);
  const bool vecEyeNotNull = vecEye.SquareMagnitude() > gp::Resolution();
  const gp_Dir dirEye(vecEyeNotNull ? vecEye : gp_Vec{0, 0, 1});

  const gp_Pln planeView(pntAt, dirEye);
  double px, py, pz;
  internal_->view->Convert(theX, theY, px, py, pz);
  const gp_Pnt pntConverted(px, py, pz);
  const gp_Pnt2d pntConvertedOnPlane =
      ProjLib::Project(planeView, pntConverted);
  return ElSLib::Value(pntConvertedOnPlane.X(), pntConvertedOnPlane.Y(),
                       planeView);
}

void OcctRenderClient::configureHighlightStyle(const Handle(Prs3d_Drawer) &
                                           theDrawer) {
  Handle(Prs3d_ShadingAspect) shadingAspect = new Prs3d_ShadingAspect();
  shadingAspect->SetColor(internal_->highlightColor);
  shadingAspect->SetMaterial(Graphic3d_NOM_PLASTIC);
  shadingAspect->SetTransparency(0);
  shadingAspect->Aspect()->SetPolygonOffsets((int)Aspect_POM_Fill, 0.99f, 0.0f);

  theDrawer->SetShadingAspect(shadingAspect);
  theDrawer->SetDisplayMode(AIS_Shaded);
}

Handle(Prs3d_Drawer) OcctRenderClient::getDefaultAISDrawer() {
  // Normal mode drawer
  Handle(Prs3d_ShadingAspect) shadingAspect = new Prs3d_ShadingAspect();
  shadingAspect->SetColor(internal_->faceColor);
  shadingAspect->SetMaterial(Graphic3d_NOM_PLASTIC);
  shadingAspect->SetTransparency(0);
  shadingAspect->Aspect()->SetPolygonOffsets((int)Aspect_POM_Fill, 0.99f, 0.0f);

  Handle(Prs3d_LineAspect) lineAspect = new Prs3d_LineAspect(
      internal_->edgeColor, Aspect_TOL_SOLID, internal_->boundaryEdgeWidth);

  Handle(Prs3d_Drawer) drawer = new Prs3d_Drawer();
  drawer->SetShadingAspect(shadingAspect);
  drawer->SetLineAspect(lineAspect);
  drawer->SetSeenLineAspect(lineAspect);
  drawer->SetWireAspect(lineAspect);
  drawer->SetFaceBoundaryAspect(lineAspect);
  drawer->SetFreeBoundaryAspect(lineAspect);
  drawer->SetUnFreeBoundaryAspect(lineAspect);
  drawer->SetFaceBoundaryUpperContinuity(GeomAbs_C2);
  drawer->SetPointAspect(new Prs3d_PointAspect(
      Aspect_TOM_O_POINT, internal_->vertexColor, internal_->vertexSize));

  drawer->SetFaceBoundaryDraw(true);
  drawer->SetDisplayMode(AIS_Shaded);
  drawer->SetTypeOfDeflection(Aspect_TOD_RELATIVE);
  return drawer;
}

// gRPC Integration Methods
bool OcctRenderClient::initGeometryClient() {
  std::lock_guard<std::mutex> lock(internal_->geometryClientMutex);
  
  // Check if already connected
  if (internal_->geometryClient && internal_->geometryClient->IsConnected()) {
    internal_->connectionStatus = ViewInternal::ConnectionStatus::Connected;
    return true;
  }
  
  internal_->connectionStatus = ViewInternal::ConnectionStatus::Connecting;
  internal_->connectionErrorMessage.clear();
  
  try {
    // Save current OpenGL context to restore later
    GLFWwindow* currentContext = glfwGetCurrentContext();
    
    // Create geometry client if not exists
    if (!internal_->geometryClient) {
      internal_->geometryClient = std::make_unique<GeometryClient>("localhost:50051");
    }
    
    // Create performance panel if not exists
    if (!internal_->performancePanel && internal_->geometryClient) {
      // Convert unique_ptr to shared_ptr for the performance panel
      std::shared_ptr<GeometryClient> sharedClient(internal_->geometryClient.get(), [](GeometryClient*){
        // Empty deleter since the unique_ptr will manage the lifetime
      });
      internal_->performancePanel = std::make_unique<GrpcPerformancePanel>(sharedClient);
    }
    
    // Ensure we're back to our OpenGL context before proceeding
    if (currentContext && currentContext == internal_->glfwWindow) {
      glfwMakeContextCurrent(internal_->glfwWindow);
    }
    
    // Connect to server with error handling
    bool connected = false;
    try {
      connected = internal_->geometryClient->Connect();
    } catch (const std::exception& e) {
      internal_->connectionErrorMessage = e.what();
      connected = false;
    }
    
    if (connected) {
      spdlog::info("OcctRenderClient: Successfully connected to geometry server");
      internal_->connectionStatus = ViewInternal::ConnectionStatus::Connected;
      internal_->connectionErrorMessage.clear();
      internal_->lastConnectionAttemptTime = static_cast<float>(glfwGetTime());
      return true;
    } else {
      internal_->connectionStatus = ViewInternal::ConnectionStatus::Disconnected;
      if (internal_->connectionErrorMessage.empty()) {
        internal_->connectionErrorMessage = "Failed to connect to localhost:50051";
      }
      internal_->lastConnectionAttemptTime = static_cast<float>(glfwGetTime());
      return false;
    }
    
  } catch (const std::exception& e) {
    internal_->connectionStatus = ViewInternal::ConnectionStatus::Error;
    internal_->connectionErrorMessage = e.what();
    internal_->lastConnectionAttemptTime = static_cast<float>(glfwGetTime());
    return false;
  } catch (...) {
    internal_->connectionStatus = ViewInternal::ConnectionStatus::Error;
    internal_->connectionErrorMessage = "Unknown error";
    internal_->lastConnectionAttemptTime = static_cast<float>(glfwGetTime());
    return false;
  }
}

void OcctRenderClient::startAsyncConnection() {
  // Check if already connecting or connected
  if (internal_->isConnecting.load() || 
      (internal_->geometryClient && internal_->geometryClient->IsConnected())) {
    return;
  }
  
  // Set status to connecting
  internal_->isConnecting.store(true);
  internal_->connectionStatus = ViewInternal::ConnectionStatus::Connecting;
  
  // Launch async connection task
  internal_->connectionFuture = std::async(std::launch::async, [this]() {
    bool result = false;
    
    try {
      // Create client if needed
      if (!internal_->geometryClient) {
        internal_->geometryClient = std::make_unique<GeometryClient>("localhost:50051");
      }
      
      // Create performance panel if not exists
      if (!internal_->performancePanel && internal_->geometryClient) {
        // Convert unique_ptr to shared_ptr for the performance panel
        std::shared_ptr<GeometryClient> sharedClient(internal_->geometryClient.get(), [](GeometryClient*){
          // Empty deleter since the unique_ptr will manage the lifetime
        });
        internal_->performancePanel = std::make_unique<GrpcPerformancePanel>(sharedClient);
      }
      
      // Attempt connection
      result = internal_->geometryClient->Connect();
      
      if (result) {
        internal_->connectionStatus = ViewInternal::ConnectionStatus::Connected;
        internal_->connectionErrorMessage.clear();
        spdlog::info("OcctRenderClient: Async connection succeeded");
      } else {
        internal_->connectionStatus = ViewInternal::ConnectionStatus::Disconnected;
        if (internal_->connectionErrorMessage.empty()) {
          internal_->connectionErrorMessage = "Failed to connect to localhost:50051";
        }
        spdlog::debug("OcctRenderClient: Async connection failed");
      }
    } catch (const std::exception& e) {
      internal_->connectionStatus = ViewInternal::ConnectionStatus::Error;
      internal_->connectionErrorMessage = e.what();
      spdlog::debug("OcctRenderClient: Async connection error: {}", e.what());
      result = false;
    }
    
    internal_->isConnecting.store(false);
    internal_->lastConnectionAttemptTime = static_cast<float>(glfwGetTime());
    
    return result;
  });
}

void OcctRenderClient::shutdownGeometryClient() {
  std::lock_guard<std::mutex> lock(internal_->geometryClientMutex);
  
  if (!internal_->geometryClient || internal_->isShuttingDown.load()) {
    return;
  }
  
  internal_->isShuttingDown.store(true);
  
  spdlog::info("OcctRenderClient: Shutting down gRPC geometry client safely...");
  
  try {
    // Save current OpenGL context
    GLFWwindow* currentContext = glfwGetCurrentContext();
    
    // Disconnect gRPC client
    if (internal_->geometryClient->IsConnected()) {
      internal_->geometryClient->Disconnect();
    }
    
    // Give gRPC threads time to finish cleanly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Reset the client
    internal_->geometryClient.reset();
    
    // Restore OpenGL context if it was valid
    if (currentContext == internal_->glfwWindow) {
      glfwMakeContextCurrent(internal_->glfwWindow);
    }
    
    spdlog::info("OcctRenderClient: gRPC geometry client shutdown completed");
    
  } catch (const std::exception& e) {
    spdlog::warn("OcctRenderClient: Exception during gRPC client shutdown: {}", e.what());
    internal_->geometryClient.reset(); // Force cleanup
  } catch (...) {
    spdlog::warn("OcctRenderClient: Unknown exception during gRPC client shutdown");
    internal_->geometryClient.reset(); // Force cleanup
  }
  
  // Reset the shutting down flag so reconnection is possible
  internal_->isShuttingDown.store(false);
}

void OcctRenderClient::createRandomBox() {
  if (!internal_->geometryClient || !internal_->geometryClient->IsConnected()) {
    spdlog::warn("OcctRenderClient::createRandomBox(): Geometry client not connected");
    return;
  }
  
  float x = static_cast<float>(rand()) / RAND_MAX * 100.0f - 50.0f;
  float y = static_cast<float>(rand()) / RAND_MAX * 100.0f - 50.0f;
  float z = 0.0f;
  float size = 10.0f + static_cast<float>(rand()) / RAND_MAX * 20.0f;
  
  try {
    std::string shape_id = internal_->geometryClient->CreateBox(x, y, z, size, size, size);
    if (!shape_id.empty()) {
      spdlog::info("OcctRenderClient: Created box with ID: {}", shape_id);
      // Get mesh data and convert to OCCT shape for display
      auto mesh_data = internal_->geometryClient->GetMeshData(shape_id);
      if (!mesh_data.vertices.empty()) {
        addMeshAsAisShape(mesh_data);
      }
    }
  } catch (const std::exception& e) {
    spdlog::error("OcctRenderClient::createRandomBox(): Error: {}", e.what());
  }
}

void OcctRenderClient::createRandomCone() {
  if (!internal_->geometryClient || !internal_->geometryClient->IsConnected()) {
    spdlog::warn("OcctRenderClient::createRandomCone(): Geometry client not connected");
    return;
  }
  
  float x = static_cast<float>(rand()) / RAND_MAX * 100.0f - 50.0f;
  float y = static_cast<float>(rand()) / RAND_MAX * 100.0f - 50.0f;
  float z = 0.0f;
  float base_radius = 5.0f + static_cast<float>(rand()) / RAND_MAX * 15.0f;
  float top_radius = static_cast<float>(rand()) / RAND_MAX * base_radius * 0.5f;
  float height = 10.0f + static_cast<float>(rand()) / RAND_MAX * 30.0f;
  
  try {
    std::string shape_id = internal_->geometryClient->CreateCone(x, y, z, base_radius, top_radius, height);
    if (!shape_id.empty()) {
      spdlog::info("OcctRenderClient: Created cone with ID: {}", shape_id);
      // Get mesh data and convert to OCCT shape for display
      auto mesh_data = internal_->geometryClient->GetMeshData(shape_id);
      if (!mesh_data.vertices.empty()) {
        addMeshAsAisShape(mesh_data);
      }
    }
  } catch (const std::exception& e) {
    spdlog::error("OcctRenderClient::createRandomCone(): Error: {}", e.what());
  }
}

void OcctRenderClient::createDemoScene() {
  if (!internal_->geometryClient || !internal_->geometryClient->IsConnected()) {
    spdlog::warn("OcctRenderClient::createDemoScene(): Geometry client not connected");
    return;
  }
  
  try {
    internal_->geometryClient->CreateDemoScene();
    spdlog::info("OcctRenderClient: Created demo scene on server");
    refreshMeshes();
  } catch (const std::exception& e) {
    spdlog::error("OcctRenderClient::createDemoScene(): Error: {}", e.what());
  }
}

void OcctRenderClient::refreshMeshes() {
  if (!internal_->geometryClient || !internal_->geometryClient->IsConnected()) {
    spdlog::warn("OcctRenderClient::refreshMeshes(): Geometry client not connected");
    return;
  }
  
  try {
    spdlog::info("OcctRenderClient: Refreshing meshes from server...");
    
    // Clear existing objects
    if (!internal_->context.IsNull()) {
      internal_->context->RemoveAll(false);
    }
    
    // Get all meshes from server
    auto all_meshes = internal_->geometryClient->GetAllMeshes();
    
    for (const auto& mesh_data : all_meshes) {
      if (!mesh_data.vertices.empty()) {
        try {
          addMeshAsAisShape(mesh_data);
        } catch (const std::exception& e) {
          spdlog::warn("OcctRenderClient::refreshMeshes(): Failed to add mesh as AIS shape: {}", e.what());
        } catch (...) {
          spdlog::warn("OcctRenderClient::refreshMeshes(): Unknown exception when adding mesh");
        }
      }
    }
    
    // Update view
    if (!internal_->view.IsNull()) {
      internal_->view->FitAll();
      internal_->view->Redraw();
    }
    
    spdlog::info("OcctRenderClient: Refreshed {} meshes", all_meshes.size());
  } catch (const std::exception& e) {
    spdlog::error("OcctRenderClient::refreshMeshes(): Error: {}", e.what());
  }
}

void OcctRenderClient::clearAllShapes() {
  try {
    if (internal_->geometryClient && internal_->geometryClient->IsConnected()) {
      internal_->geometryClient->ClearAll();
    }
    
    if (!internal_->context.IsNull()) {
      internal_->context->RemoveAll(false);
    }
    
    if (!internal_->view.IsNull()) {
      internal_->view->Redraw();
    }
    
    spdlog::info("OcctRenderClient: Cleared all shapes");
  } catch (const std::exception& e) {
    spdlog::error("OcctRenderClient::clearAllShapes(): Error: {}", e.what());
  }
}

// Template method addMeshAsAisShape is now implemented in the header file
