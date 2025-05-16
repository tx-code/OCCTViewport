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
#include <AIS_DisplayMode.hxx>
#include <Prs3d_TypeOfHighlight.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// OCCT
#include <AIS_AnimationCamera.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_ViewCube.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_Handle.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <ElSLib.hxx>
#include <Graphic3d_AspectFillArea3d.hxx>
#include <Graphic3d_MaterialAspect.hxx>
#include <OpenGl_Context.hxx>
#include <OpenGl_FrameBuffer.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <OpenGl_Texture.hxx>
#include <ProjLib.hxx>
#include <Prs3d_LineAspect.hxx>
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
  //! Flag to indicate if custom highlight style is configured
  bool configureCustomHighlightStyle{true};
  //! Flag to indicate if default drawer is configured
  bool configureDefaultDrawer{true};
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

  initViewer();
  initDemoScene();
  if (internal_->view.IsNull()) {
    return;
  }

  initGui();
  mainloop();
  cleanup();
}

void GlfwOcctView::initViewer() {
  if (!internal_->glfwWindow || internal_->occtAspectWindow.IsNull()) {
    return;
  }

  Handle(Aspect_DisplayConnection) aDispConn;
#if !defined(_WIN32) && !defined(__APPLE__) // For X11
  aDispConn = internal_->occtAspectWindow->GetDisplay();
#else
  aDispConn = new Aspect_DisplayConnection();
#endif

  Handle(OpenGl_GraphicDriver) aGraphicDriver =
      new OpenGl_GraphicDriver(aDispConn, Standard_False);
  aGraphicDriver->SetBuffersNoSwap(Standard_True);

  Handle(V3d_Viewer) aViewer = new V3d_Viewer(aGraphicDriver);
  aViewer->SetDefaultLights();
  aViewer->SetLightOn();
  aViewer->SetDefaultTypeOfView(V3d_ORTHOGRAPHIC);
  aViewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
  internal_->view = aViewer->CreateView();
  internal_->view->SetImmediateUpdate(Standard_False);

  Aspect_RenderingContext aNativeGlContext = NULL;
#if defined(_WIN32)
  aNativeGlContext = glfwGetWGLContext(internal_->glfwWindow);
#elif defined(__APPLE__)
  aNativeGlContext =
      (Aspect_RenderingContext)glfwGetNSGLContext(internal_->glfwWindow);
#else // Linux/X11
  aNativeGlContext = glfwGetGLXContext(internal_->glfwWindow);
#endif
  internal_->view->SetWindow(internal_->occtAspectWindow, aNativeGlContext);

  internal_->view->ChangeRenderingParams().ToShowStats = Standard_True;
  internal_->view->ChangeRenderingParams().CollectedStats =
      Graphic3d_RenderingParams::PerfCounters_All;

  internal_->glContext = aGraphicDriver->GetSharedContext();
  if (internal_->glContext.IsNull()) {
    spdlog::error("GlfwOcctView: Failed to get OpenGl_Context.");
  }

  initOffscreenRendering();

  internal_->context = new AIS_InteractiveContext(aViewer);

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

  /// Additional initialization
  if (internal_->configureDefaultDrawer) {
    setupDefaultAISDrawer();
  }

  if (internal_->configureCustomHighlightStyle) {
    spdlog::info(
        "GlfwOcctView::initViewer(): Configuring custom highlight style.");
    // light blue
    Quantity_Color highlightColor(128.0 / 255.0, 200.0 / 255.0, 255.0 / 255.0,
                                  Quantity_TOC_RGB);
    configureHighlightStyle(
        internal_->context->HighlightStyle(Prs3d_TypeOfHighlight_LocalSelected),
        highlightColor);
    configureHighlightStyle(
        internal_->context->HighlightStyle(Prs3d_TypeOfHighlight_Selected),
        highlightColor);
  }
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

void GlfwOcctView::resizeOffscreenFramebuffer(int theWidth, int theHeight) {
  if (internal_->glContext.IsNull() || internal_->offscreenFBO.IsNull()) {
    return;
  }

  if (internal_->needToResizeFBO ||
      internal_->offscreenFBO->GetSizeX() != theWidth ||
      internal_->offscreenFBO->GetSizeY() != theHeight) {
    internal_->offscreenFBO->InitLazy(internal_->glContext,
                                      Graphic3d_Vec2i(theWidth, theHeight),
                                      GL_RGB8, GL_DEPTH24_STENCIL8);
    internal_->needToResizeFBO = false;
  }
}

void GlfwOcctView::initGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGui_ImplGlfw_InitForOpenGL(internal_->glfwWindow, true);
  ImGui_ImplOpenGL3_Init("#version 330");
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

  float infoWidth = aIO.DisplaySize.x * 0.25f;
  float viewportWidth = aIO.DisplaySize.x - infoWidth;

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(infoWidth, aIO.DisplaySize.y));
  if (ImGui::Begin("Render Info", nullptr,
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoCollapse)) {
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
    // FIXME: if we activate the selection mode for each object except the view cube, we don't need 
    // to activate the view cube here
    internal_->context->Activate(internal_->viewCube);
  }
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(infoWidth, 0));
  ImGui::SetNextWindowSize(ImVec2(viewportWidth, aIO.DisplaySize.y));
  if (ImGui::Begin("OCCT Viewport", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                       ImGuiWindowFlags_NoScrollWithMouse |
                       ImGuiWindowFlags_NoBringToFrontOnFocus)) {
    ImVec2 aWindowSize = ImGui::GetContentRegionAvail();

    if (internal_->renderWidth != (int)aWindowSize.x ||
        internal_->renderHeight != (int)aWindowSize.y) {
      internal_->renderWidth = (int)aWindowSize.x;
      internal_->renderHeight = (int)aWindowSize.y;
      internal_->needToResizeFBO = true;

      if (!internal_->occtAspectWindow.IsNull()) {
#if defined(_WIN32)
        Handle(WNT_Window)::DownCast(internal_->occtAspectWindow)
            ->SetPos(0, 0, internal_->renderWidth, internal_->renderHeight);
#elif defined(__APPLE__)
        spdlog::debug("Cocoa_Window size update would be here if SetSize is "
                      "available and needed.");
#else // Linux/X11
        Handle(Xw_Window)::DownCast(internal_->occtAspectWindow)
            ->SetSize(internal_->renderWidth, internal_->renderHeight);
#endif
        if (!internal_->view.IsNull()) {
          internal_->view->MustBeResized();
        }
      }

      if (!internal_->view.IsNull()) {
        Handle(Graphic3d_Camera) aCamera = internal_->view->Camera();
        aCamera->SetAspect((float)internal_->renderWidth /
                           (float)internal_->renderHeight);
      }
      internal_->viewport = aWindowSize;
    }

    resizeOffscreenFramebuffer(internal_->renderWidth, internal_->renderHeight);

    if (!internal_->view.IsNull())
      internal_->view->Redraw();

    internal_->viewPos = ImGui::GetCursorScreenPos();

    if (!internal_->offscreenFBO.IsNull() &&
        internal_->offscreenFBO->ColorTexture()->TextureId() != 0) {
      ImGui::Image(
          (ImTextureID)(uintptr_t)internal_->offscreenFBO->ColorTexture()
              ->TextureId(),
          aWindowSize, ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));

      internal_->renderWindowHasFocus = ImGui::IsItemHovered();
    }
  }
  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
  internal_->context->Display(aBox, AIS_Shaded, 0, false);

  anAxis.SetLocation(gp_Pnt(25.0, 125.0, 0.0));
  Handle(AIS_Shape) aCone =
      new AIS_Shape(BRepPrimAPI_MakeCone(anAxis, 25, 0, 50).Shape());
  internal_->context->Display(aCone, AIS_Shaded, 0, false);

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

// copy from mayo: MainWindow.cpp
void GlfwOcctView::configureHighlightStyle(const Handle(Prs3d_Drawer) &
                                               theDrawer,
                                           Quantity_Color fillAreaColor) {
  // Create a new AspectFillArea3d for the highlight style
  auto fillArea = new Graphic3d_AspectFillArea3d;

  // Try to copy properties from the default shading aspect
  // This means the highlighted object will retain some of its original
  // appearance (like material) but its color will be overridden by the
  // highlight color.
  auto defaultShadingAspect =
      internal_->context->DefaultDrawer()->ShadingAspect();
  if (defaultShadingAspect && defaultShadingAspect->Aspect())
    *fillArea =
        *defaultShadingAspect->Aspect(); // Copy existing aspect settings

  // Set the interior color of the fill area
  fillArea->SetInteriorColor(fillAreaColor);

  // Define a material for the fill area (e.g., PLASTER)
  Graphic3d_MaterialAspect fillMaterial(Graphic3d_NOM_PLASTER);
  fillMaterial.SetColor(fillAreaColor); // Set material color
  // fillMaterial.SetTransparency(0.1f); // Optional: can set transparency

  // Apply the material to both front and back faces
  fillArea->SetFrontMaterial(fillMaterial);
  fillArea->SetBackMaterial(fillMaterial);

  // Set the display mode for the drawer to Shaded (AIS_Shaded)
  // This ensures the object is rendered as a solid, not wireframe, when
  // highlighted.
  theDrawer->SetDisplayMode(AIS_Shaded);

  // Apply the configured fillArea aspect to the drawer
  theDrawer->SetBasicFillAreaAspect(fillArea);
}

void GlfwOcctView::setupDefaultAISDrawer() {
  if (internal_->context.IsNull()) {
    spdlog::error("GlfwOcctView::setupDefaultAISDrawer(): "
                  "AIS_InteractiveContext is null.");
    return;
  }
  spdlog::info(
      "GlfwOcctView::setupDefaultAISDrawer(): Configuring default AIS drawer.");

  // FIXME: these values should be configurable
  static const Standard_Real sLineWidth = 2.0;
  static const Quantity_Color sEdgeColor(0.0, 192.0 / 255.0, 255.0 / 255.0,
                                         Quantity_TOC_RGB);
  static const Quantity_Color sFillColor(Quantity_NOC_GRAY90);
  static const Aspect_TypeOfLine sTypeOfLine(Aspect_TOL_SOLID);

  Handle(Prs3d_Drawer) drawer = internal_->context->DefaultDrawer();

  // Set default display mode
  drawer->SetDisplayMode(AIS_Shaded); // Default to shaded display

  // Configure edge display (Face Boundary)
  drawer->SetFaceBoundaryDraw(Standard_True); // Show edges by default

  // Configure default line aspect for edges
  Handle(Prs3d_LineAspect) lineAspect = drawer->LineAspect();
  if (lineAspect.IsNull()) {
    lineAspect = new Prs3d_LineAspect(sEdgeColor, sTypeOfLine, sLineWidth);
    drawer->SetLineAspect(lineAspect);
  } else {
    // Mayo classic theme cyan-like color for edges: QColor(0, 192, 255)
    lineAspect->SetColor(sEdgeColor);
    lineAspect->SetTypeOfLine(sTypeOfLine);
    lineAspect->SetWidth(sLineWidth);
  }

  // Configure default shading aspect (fill color and material)
  Handle(Graphic3d_AspectFillArea3d) fillAspect =
      drawer->ShadingAspect()->Aspect();
  if (fillAspect.IsNull()) {
    // This case should ideally not happen if ShadingAspect() itself is not null
    // and its Aspect() is just not set. If ShadingAspect() can be null,
    // we'd need to new Prs3d_ShadingAspect and then its aspect.
    // For simplicity, assuming ShadingAspect and its Aspect() can be
    // configured.
    Handle(Prs3d_ShadingAspect) sa = new Prs3d_ShadingAspect();
    fillAspect = new Graphic3d_AspectFillArea3d();
    sa->SetAspect(fillAspect);
    drawer->SetShadingAspect(sa);
  }
  fillAspect->SetInteriorColor(sFillColor);
  Graphic3d_MaterialAspect material(Graphic3d_NOM_PLASTIC);
  material.SetColor(sFillColor);
  fillAspect->SetFrontMaterial(material);
  fillAspect->SetBackMaterial(material);

  // Optional: Transparency, HLR settings etc. can also be set here
  // drawer->SetTransparency(0.0);
  // drawer->SetTypeOfHLR(Prs3d_TOH_PolyAlgo);
}