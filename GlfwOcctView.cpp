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

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <AIS_AnimationCamera.hxx>
#include <AIS_Shape.hxx>
#include <AIS_ViewCube.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_Handle.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <OpenGl_Texture.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <V3d_TypeOfView.hxx>
#include <spdlog/spdlog.h>

#include <GLFW/glfw3.h>

namespace {
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

// ================================================================
// Function : GlfwOcctView
// Purpose  :
// ================================================================
GlfwOcctView::GlfwOcctView() {}

// ================================================================
// Function : ~GlfwOcctView
// Purpose  :
// ================================================================
GlfwOcctView::~GlfwOcctView() {}

// ================================================================
// Function : toView
// Purpose  :
// ================================================================
GlfwOcctView *GlfwOcctView::toView(GLFWwindow *theWin) {
  return static_cast<GlfwOcctView *>(glfwGetWindowUserPointer(theWin));
}

// ================================================================
// Function : errorCallback
// Purpose  :
// ================================================================
void GlfwOcctView::errorCallback(int theError, const char *theDescription) {
  Message::DefaultMessenger()->Send(TCollection_AsciiString("Error") +
                                        theError + ": " + theDescription,
                                    Message_Fail);
}

// ================================================================
// Function : run
// Purpose  :
// ================================================================
void GlfwOcctView::run() {
  initWindow(1920, 1080, "OCCT IMGUI");
  initViewer();
  initDemoScene();
  if (myView.IsNull()) {
    return;
  }

  // 获取窗口实际大小
  int aWidth, aHeight;
  glfwGetWindowSize(myOcctWindow->getGlfwWindow(), &aWidth, &aHeight);
  myRenderWidth = aWidth;
  myRenderHeight = aHeight;
  myViewport = ImVec2(aWidth, aHeight);

  myView->MustBeResized();
  myOcctWindow->Map();
  initGui();
  mainloop();
  cleanup();
}

// ================================================================
// Function : initWindow
// Purpose  :
// ================================================================
void GlfwOcctView::initWindow(int theWidth, int theHeight,
                              const char *theTitle) {
  glfwSetErrorCallback(GlfwOcctView::errorCallback);
  glfwInit();
  const bool toAskCoreProfile = true;
  if (toAskCoreProfile) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, true);
    // glfwWindowHint(GLFW_DECORATED, GL_FALSE);
  }
  myOcctWindow = new GlfwOcctWindow(theWidth, theHeight, theTitle);
  glfwSetWindowUserPointer(myOcctWindow->getGlfwWindow(), this);

  // window callback
  glfwSetWindowSizeCallback(myOcctWindow->getGlfwWindow(),
                            GlfwOcctView::onResizeCallback);
  glfwSetFramebufferSizeCallback(myOcctWindow->getGlfwWindow(),
                                 GlfwOcctView::onFBResizeCallback);

  // mouse callback
  glfwSetScrollCallback(myOcctWindow->getGlfwWindow(),
                        GlfwOcctView::onMouseScrollCallback);
  glfwSetMouseButtonCallback(myOcctWindow->getGlfwWindow(),
                             GlfwOcctView::onMouseButtonCallback);
  glfwSetCursorPosCallback(myOcctWindow->getGlfwWindow(),
                           GlfwOcctView::onMouseMoveCallback);
}

// ================================================================
// Function : initViewer
// Purpose  :
// ================================================================
void GlfwOcctView::initViewer() {
  if (myOcctWindow.IsNull() || myOcctWindow->getGlfwWindow() == nullptr) {
    return;
  }

  Handle(OpenGl_GraphicDriver) aGraphicDriver =
      new OpenGl_GraphicDriver(myOcctWindow->GetDisplay(), Standard_False);
  aGraphicDriver->SetBuffersNoSwap(Standard_True);

  Handle(V3d_Viewer) aViewer = new V3d_Viewer(aGraphicDriver);
  aViewer->SetDefaultLights();
  aViewer->SetLightOn();
  aViewer->SetDefaultTypeOfView(V3d_ORTHOGRAPHIC);
  aViewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
  myView = aViewer->CreateView();
  myView->SetImmediateUpdate(Standard_False);
  myView->SetWindow(myOcctWindow, myOcctWindow->NativeGlContext());
  myView->ChangeRenderingParams().ToShowStats = Standard_True;
  myView->ChangeRenderingParams().CollectedStats =
      Graphic3d_RenderingParams::PerfCounters_All;

  // 获取OpenGL上下文
  myGLContext = aGraphicDriver->GetSharedContext();

  // 初始化离屏渲染
  initOffscreenRendering();

  myContext = new AIS_InteractiveContext(aViewer);

  myViewCube = new AIS_ViewCube();
  myViewCube->SetSize(55);
  myViewCube->SetFontHeight(12);
  myViewCube->SetAxesLabels("", "", "");
  myViewCube->SetTransformPersistence(new Graphic3d_TransformPers(
      Graphic3d_TMF_TriedronPers, Aspect_TOTP_LEFT_LOWER,
      Graphic3d_Vec2i(100, 100)));
  myViewCube->SetViewAnimation(this->ViewAnimation());
  if (myFixedViewCubeAnimationLoop) {
    myViewCube->SetDuration(0.1);
    myViewCube->SetFixedAnimationLoop(true);
  } else {
    myViewCube->SetDuration(0.5);
    myViewCube->SetFixedAnimationLoop(false);
  }
  myContext->Display(myViewCube, false);
}

// ================================================================
// Function : initOffscreenRendering
// Purpose  : Initialize off-screen framebuffer for rendering
// ================================================================
void GlfwOcctView::initOffscreenRendering() {
  if (myGLContext.IsNull() || myView.IsNull()) {
    return;
  }

  // 获取窗口实际大小
  int aWidth, aHeight;
  glfwGetWindowSize(myOcctWindow->getGlfwWindow(), &aWidth, &aHeight);

  // 确保渲染大小与窗口大小一致
  myRenderWidth = aWidth;
  myRenderHeight = aHeight;

  // 创建帧缓冲对象
  myOffscreenFBO = Handle(OpenGl_FrameBuffer)::DownCast(
      myView->View()->FBOCreate(myRenderWidth, myRenderHeight));
  if (!myOffscreenFBO.IsNull()) {
    // 设置纹理过滤模式
    myOffscreenFBO->ColorTexture()->Sampler()->Parameters()->SetFilter(
        Graphic3d_TOTF_BILINEAR);
    // 将FBO绑定到视图
    myView->View()->SetFBO(myOffscreenFBO);
  }
}

// ================================================================
// Function : resizeOffscreenFramebuffer
// Purpose  : Resize off-screen framebuffer if needed
// ================================================================
void GlfwOcctView::resizeOffscreenFramebuffer(int theWidth, int theHeight) {
  if (myGLContext.IsNull() || myOffscreenFBO.IsNull()) {
    return;
  }

  if (myNeedToResizeFBO || myOffscreenFBO->GetSizeX() != theWidth ||
      myOffscreenFBO->GetSizeY() != theHeight) {
    // 重新初始化FBO
    myOffscreenFBO->InitLazy(myGLContext, Graphic3d_Vec2i(theWidth, theHeight),
                             GL_RGB8, GL_DEPTH24_STENCIL8);
    myNeedToResizeFBO = false;
  }
}

void GlfwOcctView::initGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGui_ImplGlfw_InitForOpenGL(myOcctWindow->getGlfwWindow(), true);
  ImGui_ImplOpenGL3_Init("#version 330");
}

void GlfwOcctView::renderGui() {
  ImGuiIO &aIO = ImGui::GetIO();

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();

  // 获取当前鼠标位置（调整前）
  Graphic3d_Vec2i aMousePos = myOcctWindow->CursorPosition();
  // 计算调整后的鼠标位置
  Graphic3d_Vec2i aAdjustedMousePos =
      adjustMousePosition(aMousePos.x(), aMousePos.y());

  // 计算左侧信息窗口和右侧视图窗口的尺寸
  float infoWidth = aIO.DisplaySize.x * 0.25f;         // 1/4 屏幕宽度
  float viewportWidth = aIO.DisplaySize.x - infoWidth; // 3/4 屏幕宽度

  // 左侧信息窗口
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(infoWidth, aIO.DisplaySize.y));
  if (ImGui::Begin("Render Info", nullptr,
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoCollapse)) {
    ImGui::Text("Render Size: %d x %d", myRenderWidth, myRenderHeight);
    ImGui::Text("Need Resize FBO: %s", myNeedToResizeFBO ? "Yes" : "No");
    ImGui::Text("Viewport Size: %.1f x %.1f", myViewport.x, myViewport.y);
    ImGui::Text("View Position: %.1f, %.1f", myViewPos.x, myViewPos.y);
    ImGui::Text("Mouse Over Render Area: %s",
                myRenderWindowHasFocus ? "Yes" : "No");

    // 显示鼠标位置信息
    ImGui::Separator();
    ImGui::Text("Mouse Position (Original): %d, %d", aMousePos.x(),
                aMousePos.y());
    ImGui::Text("Mouse Position (Adjusted): %d, %d", aAdjustedMousePos.x(),
                aAdjustedMousePos.y());
    if (myRenderWindowHasFocus) {
      ImGui::Text("Mouse Offset: %d, %d", aMousePos.x() - aAdjustedMousePos.x(),
                  aMousePos.y() - aAdjustedMousePos.y());
    }

    if (myRenderWindowHasFocus) {
      // 显示AIS_ViewController鼠标状态相关变量
      ImGui::Separator();
      ImGui::Text("Mouse States:");

      ImGui::Text("Last Mouse Position: %d, %d", LastMousePosition().x(),
                  LastMousePosition().y());

      // 显示当前按下的鼠标按钮
      Aspect_VKeyMouse aButtons = PressedMouseButtons();
      ImGui::Text("Pressed Mouse Buttons: 0x%X", aButtons);
      ImGui::Text("- Left: %s",
                  (aButtons & Aspect_VKeyMouse_LeftButton) ? "Yes" : "No");
      ImGui::Text("- Right: %s",
                  (aButtons & Aspect_VKeyMouse_RightButton) ? "Yes" : "No");
      ImGui::Text("- Middle: %s",
                  (aButtons & Aspect_VKeyMouse_MiddleButton) ? "Yes" : "No");

      // 显示最后一次记录的鼠标标志
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

    // 显示事件计时器信息 - EventsTimer不是AIS_ViewController的公共方法或属性
    // 使用自定义方式显示
    ImGui::Text("Events Time Info:");
    ImGui::Text("- Last Event Time: %.2f", EventTime());

    // 显示myToAskNextFrame
    ImGui::Separator();
    ImGui::Text("ToAskNextFrame: %s", myToAskNextFrame ? "Yes" : "No");

    // Display framerate
    ImGui::Separator();
    ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);

    // Controls
    ImGui::Separator();
    if (ImGui::Checkbox("Fixed View Cube Animation Loop",
                        &myFixedViewCubeAnimationLoop)) {
      if (myFixedViewCubeAnimationLoop) {
        myViewCube->SetDuration(0.1);
        myViewCube->SetFixedAnimationLoop(true);
      } else {
        myViewCube->SetDuration(0.5);
        myViewCube->SetFixedAnimationLoop(false);
      }
    }
  }
  ImGui::End();

  static bool showDemoWindow = true;
  ImGui::ShowDemoWindow(&showDemoWindow);

  // 右侧视图窗口
  ImGui::SetNextWindowPos(ImVec2(infoWidth, 0));
  ImGui::SetNextWindowSize(ImVec2(viewportWidth, aIO.DisplaySize.y));
  if (ImGui::Begin("OCCT Viewport", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                       ImGuiWindowFlags_NoScrollWithMouse |
                       ImGuiWindowFlags_NoBringToFrontOnFocus)) {
    // 获取窗口内容区域大小
    ImVec2 aWindowSize = ImGui::GetContentRegionAvail();

    // 设置渲染尺寸为窗口尺寸
    if (myRenderWidth != (int)aWindowSize.x ||
        myRenderHeight != (int)aWindowSize.y) {
      myRenderWidth = (int)aWindowSize.x;
      myRenderHeight = (int)aWindowSize.y;
      myNeedToResizeFBO = true;

      // 更新相机纵横比
      if (!myView.IsNull()) {
        Handle(Graphic3d_Camera) aCamera = myView->Camera();
        aCamera->SetAspect((float)myRenderWidth / (float)myRenderHeight);
      }

      myViewport = aWindowSize;
    }

    // 根据需要调整FBO大小
    resizeOffscreenFramebuffer(myRenderWidth, myRenderHeight);

    // 重绘视图
    myView->Redraw();

    // 获取当前位置并显示渲染纹理
    myViewPos = ImGui::GetCursorScreenPos();

    // 显示渲染结果
    if (!myOffscreenFBO.IsNull() &&
        myOffscreenFBO->ColorTexture()->TextureId() != 0) {
      ImGui::Image(
          (ImTextureID)(uintptr_t)myOffscreenFBO->ColorTexture()->TextureId(),
          aWindowSize, ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));

      // 检查鼠标是否在渲染区域内
      myRenderWindowHasFocus = ImGui::IsItemHovered();
    }
  }
  ImGui::End();

  ImGui::Render();

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  glfwSwapBuffers(myOcctWindow->getGlfwWindow());
}

// ================================================================
// Function : initDemoScene
// Purpose  :
// ================================================================
void GlfwOcctView::initDemoScene() {
  if (myContext.IsNull()) {
    return;
  }

  myView->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_GOLD, 0.08,
                          V3d_WIREFRAME);

  gp_Ax2 anAxis;
  anAxis.SetLocation(gp_Pnt(0.0, 0.0, 0.0));
  Handle(AIS_Shape) aBox =
      new AIS_Shape(BRepPrimAPI_MakeBox(anAxis, 50, 50, 50).Shape());
  myContext->Display(aBox, AIS_Shaded, 0, false);
  anAxis.SetLocation(gp_Pnt(25.0, 125.0, 0.0));
  Handle(AIS_Shape) aCone =
      new AIS_Shape(BRepPrimAPI_MakeCone(anAxis, 25, 0, 50).Shape());
  myContext->Display(aCone, AIS_Shaded, 0, false);

  TCollection_AsciiString aGlInfo;
  {
    TColStd_IndexedDataMapOfStringString aRendInfo;
    myView->DiagnosticInformation(aRendInfo, Graphic3d_DiagnosticInfo_Basic);
    for (TColStd_IndexedDataMapOfStringString::Iterator aValueIter(aRendInfo);
         aValueIter.More(); aValueIter.Next()) {
      if (!aGlInfo.IsEmpty()) {
        aGlInfo += "\n";
      }
      aGlInfo += TCollection_AsciiString("  ") + aValueIter.Key() + ": " +
                 aValueIter.Value();
    }
  }
  Message::DefaultMessenger()->Send(
      TCollection_AsciiString("OpenGL info:\n") + aGlInfo, Message_Info);
}

// ================================================================
// Function : mainloop
// Purpose  :
// ================================================================
void GlfwOcctView::mainloop() {
  while (!glfwWindowShouldClose(myOcctWindow->getGlfwWindow())) {
    // glfwPollEvents() for continuous rendering (immediate return if there are
    // no new events) and glfwWaitEvents() for rendering on demand (something
    // actually happened in the viewer)
    if (!toAskNextFrame()) {
      glfwWaitEvents();
    } else {
      glfwPollEvents();
    }
    if (!myView.IsNull()) {

      myView->InvalidateImmediate(); // redraw view even if it wasn't modified
      FlushViewEvents(myContext, myView, Standard_True);

      renderGui();
    }
  }
}

// ================================================================
// Function : cleanup
// Purpose  :
// ================================================================
void GlfwOcctView::cleanup() {
  // 释放离屏渲染资源
  if (!myOffscreenFBO.IsNull() && !myGLContext.IsNull()) {
    myOffscreenFBO->Release(myGLContext.get());
    myOffscreenFBO.Nullify();
  }

  // Cleanup IMGUI.
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (!myView.IsNull()) {
    myView->Remove();
  }
  if (!myOcctWindow.IsNull()) {
    myOcctWindow->Close();
  }

  glfwTerminate();
}

// ================================================================
// Function : onResize
// Purpose  :
// ================================================================
void GlfwOcctView::onResize(int theWidth, int theHeight) {
  spdlog::info("onResize: {} {}", theWidth, theHeight);
  if (theWidth != 0 && theHeight != 0 && !myView.IsNull()) {
    myView->Window()->DoResize();
    myView->MustBeResized();

    // 调整FBO大小
    if (myRenderWidth != theWidth || myRenderHeight != theHeight) {
      myRenderWidth = theWidth;
      myRenderHeight = theHeight;
      myNeedToResizeFBO = true;
    }

    myView->Invalidate();
    FlushViewEvents(myContext, myView, true);
    renderGui();
  }
}

// ================================================================
// Function : adjustMousePosition
// Purpose  : 根据当前视口调整鼠标位置
// ================================================================
Graphic3d_Vec2i GlfwOcctView::adjustMousePosition(int thePosX,
                                                  int thePosY) const {
  // 根据IMGUI视口位置和大小调整鼠标坐标
  if (myRenderWindowHasFocus) {
    // 如果鼠标在渲染窗口内，则需要相对于渲染视口的左上角计算
    return Graphic3d_Vec2i(thePosX - static_cast<int>(myViewPos.x),
                           thePosY - static_cast<int>(myViewPos.y));
  }

  // 如果鼠标不在渲染窗口内，则使用原始坐标
  return Graphic3d_Vec2i(thePosX, thePosY);
}

// ================================================================
// Function : onMouseButton
// Purpose  :
// ================================================================
void GlfwOcctView::onMouseButton(int theButton, int theAction, int theMods) {
  spdlog::info("onMouseButton: {} {} {}", theButton, theAction, theMods);

  if (myView.IsNull() || !myRenderWindowHasFocus) {
    return;
  }

  // 获取调整后的鼠标位置
  Graphic3d_Vec2i aPos = myOcctWindow->CursorPosition();
  Graphic3d_Vec2i aAdjustedPos = adjustMousePosition(aPos.x(), aPos.y());

  if (theAction == GLFW_PRESS) {
    if (PressMouseButton(aAdjustedPos, mouseButtonFromGlfw(theButton),
                         keyFlagsFromGlfw(theMods), false)) {
      HandleViewEvents(myContext, myView);
    }
  } else {
    if (ReleaseMouseButton(aAdjustedPos, mouseButtonFromGlfw(theButton),
                           keyFlagsFromGlfw(theMods), false)) {
      HandleViewEvents(myContext, myView);
    }
  }
}

// ================================================================
// Function : onMouseMove
// Purpose  :
// ================================================================
void GlfwOcctView::onMouseMove(int thePosX, int thePosY) {
  spdlog::info("onMouseMove: {} {}", thePosX, thePosY);

  if (myView.IsNull() || !myRenderWindowHasFocus) {
    return;
  }

  // 使用adjustMousePosition函数获取调整后的鼠标位置
  Graphic3d_Vec2i aAdjustedPos = adjustMousePosition(thePosX, thePosY);
  if (UpdateMousePosition(aAdjustedPos, PressedMouseButtons(), LastMouseFlags(),
                          Standard_False)) {
    HandleViewEvents(myContext, myView);
  }
}

// ================================================================
// Function : onMouseScroll
// Purpose  :
// ================================================================
void GlfwOcctView::onMouseScroll(double theOffsetX, double theOffsetY) {
  spdlog::info("onMouseScroll: {} {}", theOffsetX, theOffsetY);

  if (myView.IsNull() || !myRenderWindowHasFocus) {
    return;
  }

  // 获取调整后的鼠标位置
  Graphic3d_Vec2i aPos = myOcctWindow->CursorPosition();
  Graphic3d_Vec2i aAdjustedPos = adjustMousePosition(aPos.x(), aPos.y());
  if (UpdateZoom(Aspect_ScrollDelta(aAdjustedPos, int(theOffsetY * 8.0)))) {
    HandleViewEvents(myContext, myView);
  }
}
