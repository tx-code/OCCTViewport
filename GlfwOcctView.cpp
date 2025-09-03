// MIT License
//
// Copyright(c) 2023 Shing Liu
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

#include "GlfwOcctView.h"

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
struct GlfwOcctView::ViewInternal {
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

  // OCAF Demo Widget
  //! OCAF Demo Widget instance
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

GlfwOcctView::GlfwOcctView(GLFWwindow *aGlfwWindow) {
  internal_ = std::make_unique<ViewInternal>();
  internal_->glfwWindow = aGlfwWindow;

  if (!internal_->glfwWindow) {
    Message::DefaultMessenger()->Send(
        "GlfwOcctView: GLFW window is null on construction.", Message_Fail);
    return;
  }

  // so we can get the GlfwOcctView pointer from the GLFW window
  glfwSetWindowUserPointer(internal_->glfwWindow, this);

  // Set GLFW callbacks
  glfwSetWindowSizeCallback(internal_->glfwWindow,
                            GlfwOcctView::onResizeCallback);
  glfwSetFramebufferSizeCallback(internal_->glfwWindow,
                                 GlfwOcctView::onFBResizeCallback);
  glfwSetScrollCallback(internal_->glfwWindow,
                        GlfwOcctView::onMouseScrollCallback);
  glfwSetMouseButtonCallback(internal_->glfwWindow,
                             GlfwOcctView::onMouseButtonCallback);
  glfwSetCursorPosCallback(internal_->glfwWindow,
                           GlfwOcctView::onMouseMoveCallback);

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
    spdlog::error("GlfwOcctView: Failed to create OCCT Aspect_Window wrapper.");
  }
}

GlfwOcctView::~GlfwOcctView() {
  // std::unique_ptr<ViewInternal> internal_ will be automatically destroyed.
}

// Static callback implementations (were previously in .h or implied)
void GlfwOcctView::onResizeCallback(GLFWwindow *theWin, int theWidth,
                                    int theHeight) {
  toView(theWin)->onResize(theWidth, theHeight);
}
void GlfwOcctView::onFBResizeCallback(GLFWwindow *theWin, int theWidth,
                                      int theHeight) {
  toView(theWin)->onResize(theWidth, theHeight);
}
void GlfwOcctView::onMouseScrollCallback(GLFWwindow *theWin,
                                         double referidoOffsetX,
                                         double theOffsetY) {
  toView(theWin)->onMouseScroll(referidoOffsetX, theOffsetY);
}
void GlfwOcctView::onMouseButtonCallback(GLFWwindow *theWin, int theButton,
                                         int theAction, int theMods) {
  toView(theWin)->onMouseButton(theButton, theAction, theMods);
}
void GlfwOcctView::onMouseMoveCallback(GLFWwindow *theWin, double thePosX,
                                       double thePosY) {
  toView(theWin)->onMouseMove((int)thePosX, (int)thePosY);
}

GlfwOcctView *GlfwOcctView::toView(GLFWwindow *theWin) {
  return static_cast<GlfwOcctView *>(glfwGetWindowUserPointer(theWin));
}

void GlfwOcctView::errorCallback(int theError, const char *theDescription) {
  Message::DefaultMessenger()->Send(TCollection_AsciiString("Error") +
                                        theError + ": " + theDescription,
                                    Message_Fail);
}

void GlfwOcctView::run() {
  if (!internal_->glfwWindow || internal_->occtAspectWindow.IsNull()) {
    spdlog::error("GlfwOcctView::run(): Window not properly initialized.");
    return;
  }

  initOCCTRenderingSystem();
  initDemoScene();
  if (internal_->view.IsNull()) {
    return;
  }

  initGui();
  mainloop();
  cleanup();
}

void GlfwOcctView::addAisObject(const Handle(AIS_InteractiveObject) &
                                theAisObject) {
  if (theAisObject.IsNull()) {
    spdlog::error("GlfwOcctView::addAisObject(): AIS object is null.");
    return;
  }

  if (internal_->context.IsNull()) {
    spdlog::error("GlfwOcctView::addAisObject(): Context is not initialized.");
    return;
  }

  theAisObject->SetAttributes(getDefaultAISDrawer());
  internal_->context->Display(theAisObject, AIS_Shaded, 0, false);
}

void GlfwOcctView::initOCCTRenderingSystem() {
  if (!internal_->glfwWindow || internal_->occtAspectWindow.IsNull()) {
    spdlog::error("No GLFW window or OCCT Aspect_Window found.");
    return;
  }

  initV3dViewer();
  initAisContext();
  initOffscreenRendering();
  initVisualSettings();
}

void GlfwOcctView::initOffscreenRendering() {
  if (internal_->glContext.IsNull() || internal_->view.IsNull()) {
    spdlog::warn(
        "GlfwOcctView::initOffscreenRendering(): GLContext or View is Null.");
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
    spdlog::error("GlfwOcctView: Failed to create offscreen FBO.");
  }
}

bool GlfwOcctView::resizeOffscreenFramebuffer(int theWidth, int theHeight) {
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

void GlfwOcctView::initGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGui_ImplGlfw_InitForOpenGL(internal_->glfwWindow, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  // docking
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
}

void GlfwOcctView::renderGui() {
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

void GlfwOcctView::initDemoScene() {
  if (internal_->context.IsNull()) {
    return;
  }

  internal_->view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_GOLD,
                                   0.08, V3d_WIREFRAME);

  gp_Ax2 anAxis;
  anAxis.SetLocation(gp_Pnt(0.0, 0.0, 0.0));
  Handle(AIS_Shape) aBox =
      new AIS_Shape(BRepPrimAPI_MakeBox(anAxis, 50, 50, 50).Shape());
  addAisObject(aBox);

  anAxis.SetLocation(gp_Pnt(25.0, 125.0, 0.0));
  Handle(AIS_Shape) aCone =
      new AIS_Shape(BRepPrimAPI_MakeCone(anAxis, 25, 0, 50).Shape());
  addAisObject(aCone);

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

void GlfwOcctView::initV3dViewer() {
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
    spdlog::error("GlfwOcctView: Failed to get OpenGl_Context.");
    assert(false);
  }
}

void GlfwOcctView::initAisContext() {
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
      spdlog::warn("GlfwOcctView: ViewAnimation not available for ViewCube.");
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

void GlfwOcctView::initVisualSettings() {
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

void GlfwOcctView::mainloop() {
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

void GlfwOcctView::cleanup() {
  if (!internal_->offscreenFBO.IsNull() && !internal_->glContext.IsNull()) {
    internal_->offscreenFBO->Release(internal_->glContext.get());
    internal_->offscreenFBO.Nullify();
  }
  internal_->glContext.Nullify();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  if (ImGui::GetCurrentContext()) {
    ImGui::DestroyContext();
  }

  if (!internal_->view.IsNull()) {
    internal_->view->Remove();
  }
  internal_->occtAspectWindow.Nullify();
}

void GlfwOcctView::onResize(int theWidth, int theHeight) {
  if (theWidth != 0 && theHeight != 0 && !internal_->view.IsNull()) {
    internal_->view->MustBeResized();
    internal_->view->Invalidate(); // invalidate the whole view
  }
}

Graphic3d_Vec2i GlfwOcctView::adjustMousePosition(int thePosX,
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

void GlfwOcctView::onMouseButton(int theButton, int theAction, int theMods) {
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

void GlfwOcctView::onMouseMove(int thePosX, int thePosY) {
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

void GlfwOcctView::onMouseScroll(double theOffsetX, double theOffsetY) {
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

// Convert screen coordinates to 3D coordinates in the view
// copy from mayo: graphics_utils.cpp
gp_Pnt GlfwOcctView::screenToViewCoordinates(int theX, int theY) const {
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

void GlfwOcctView::configureHighlightStyle(const Handle(Prs3d_Drawer) &
                                           theDrawer) {
  Handle(Prs3d_ShadingAspect) shadingAspect = new Prs3d_ShadingAspect();
  shadingAspect->SetColor(internal_->highlightColor);
  shadingAspect->SetMaterial(Graphic3d_NOM_PLASTIC);
  shadingAspect->SetTransparency(0);
  shadingAspect->Aspect()->SetPolygonOffsets((int)Aspect_POM_Fill, 0.99f, 0.0f);

  theDrawer->SetShadingAspect(shadingAspect);
  theDrawer->SetDisplayMode(AIS_Shaded);
}

Handle(Prs3d_Drawer) GlfwOcctView::getDefaultAISDrawer() {
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
