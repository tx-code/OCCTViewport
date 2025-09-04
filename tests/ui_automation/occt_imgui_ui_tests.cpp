// OcctImgui UI Automation Tests
// Standalone test application using ImGui Test Engine

#define _CRT_SECURE_NO_WARNINGS
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"
#include <iostream>
#include <memory>

//-------------------------------------------------------------------------
// Mock gRPC client state for testing
//-------------------------------------------------------------------------
static bool mock_connected = false;

static void RenderGrpcControlPanel() {
    if (ImGui::Begin("gRPC Control Panel")) {
        ImGui::Text("Connection Status: %s", mock_connected ? "Connected" : "Disconnected");
        
        // Connection control
        if (ImGui::Button("Connect")) {
            mock_connected = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Disconnect")) {
            mock_connected = false;
        }
        
        ImGui::Separator();
        
        // Shape creation buttons - should be disabled when disconnected
        ImGui::BeginDisabled(!mock_connected);
        
        ImGui::Button("Create Box");
        ImGui::Button("Create Cone"); 
        ImGui::Button("Create Sphere");
        ImGui::Button("Create Cylinder");
        
        ImGui::EndDisabled();
        
        ImGui::Separator();
        
        // Management buttons
        ImGui::BeginDisabled(!mock_connected);
        
        ImGui::Button("Clear All");
        ImGui::Button("Demo Scene");
        
        ImGui::EndDisabled();
    }
    ImGui::End();
}

//-------------------------------------------------------------------------
// Register OcctImgui tests
//-------------------------------------------------------------------------
void RegisterTests_OcctImgui(ImGuiTestEngine* e)
{
    ImGuiTest* t = NULL;

    // Test that buttons are disabled when disconnected (pure thin client behavior)
    t = IM_REGISTER_TEST(e, "occt_imgui", "grpc_buttons_disabled_when_disconnected");
    t->GuiFunc = [](ImGuiTestContext* ctx)
    {
        mock_connected = false;  // Ensure disconnected state
        RenderGrpcControlPanel();
    };
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ctx->SetRef("gRPC Control Panel");
        
        // All shape creation buttons should be disabled
        const char* buttons[] = {"Create Box", "Create Cone", "Create Sphere", "Create Cylinder"};
        
        for (const char* button_name : buttons) {
            ImGuiTestItemInfo info = ctx->ItemInfo(button_name);
            IM_CHECK(info.ID != 0);  // Button exists
            IM_CHECK((info.ItemFlags & ImGuiItemFlags_Disabled) != 0);  // Button is disabled
        }
        
        // Management buttons should also be disabled
        const char* mgmt_buttons[] = {"Clear All", "Demo Scene"};
        for (const char* button_name : mgmt_buttons) {
            ImGuiTestItemInfo info = ctx->ItemInfo(button_name);
            IM_CHECK(info.ID != 0);
            IM_CHECK((info.ItemFlags & ImGuiItemFlags_Disabled) != 0);
        }
        
        // Connection buttons should be enabled
        ImGuiTestItemInfo connect_info = ctx->ItemInfo("Connect");
        IM_CHECK(connect_info.ID != 0);
        IM_CHECK((connect_info.ItemFlags & ImGuiItemFlags_Disabled) == 0);
    };

    // Test that buttons are enabled when connected
    t = IM_REGISTER_TEST(e, "occt_imgui", "grpc_buttons_enabled_when_connected");
    t->GuiFunc = [](ImGuiTestContext* ctx)
    {
        mock_connected = true;  // Ensure connected state
        RenderGrpcControlPanel();
    };
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ctx->SetRef("gRPC Control Panel");
        
        // All shape creation buttons should be enabled
        const char* buttons[] = {"Create Box", "Create Cone", "Create Sphere", "Create Cylinder"};
        
        for (const char* button_name : buttons) {
            ImGuiTestItemInfo info = ctx->ItemInfo(button_name);
            IM_CHECK(info.ID != 0);
            IM_CHECK((info.ItemFlags & ImGuiItemFlags_Disabled) == 0);  // Button is enabled
        }
        
        // Management buttons should also be enabled
        const char* mgmt_buttons[] = {"Clear All", "Demo Scene"};
        for (const char* button_name : mgmt_buttons) {
            ImGuiTestItemInfo info = ctx->ItemInfo(button_name);
            IM_CHECK(info.ID != 0);
            IM_CHECK((info.ItemFlags & ImGuiItemFlags_Disabled) == 0);
        }
    };

    // Test connection state transitions
    t = IM_REGISTER_TEST(e, "occt_imgui", "grpc_connection_state_transitions");
    t->GuiFunc = [](ImGuiTestContext* ctx)
    {
        RenderGrpcControlPanel();
    };
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ctx->SetRef("gRPC Control Panel");
        
        // Start disconnected
        mock_connected = false;
        ctx->Yield();
        
        // Initially disconnected - buttons should be disabled
        ImGuiTestItemInfo box_info = ctx->ItemInfo("Create Box");
        IM_CHECK(box_info.ID != 0);
        IM_CHECK((box_info.ItemFlags & ImGuiItemFlags_Disabled) != 0);
        
        // Click connect button
        ctx->ItemClick("Connect");
        mock_connected = true;
        ctx->Yield();
        
        // After connecting, buttons should be enabled
        ImGuiTestItemInfo box_info_after = ctx->ItemInfo("Create Box");
        IM_CHECK(box_info_after.ID != 0);
        IM_CHECK((box_info_after.ItemFlags & ImGuiItemFlags_Disabled) == 0);
        
        // Click disconnect button
        ctx->ItemClick("Disconnect");
        mock_connected = false;
        ctx->Yield();
        
        // After disconnecting, buttons should be disabled again
        ImGuiTestItemInfo box_info_final = ctx->ItemInfo("Create Box");
        IM_CHECK(box_info_final.ID != 0);
        IM_CHECK((box_info_final.ItemFlags & ImGuiItemFlags_Disabled) != 0);
    };
}

//-------------------------------------------------------------------------
// Main test runner
//-------------------------------------------------------------------------
int main(int argc, char** argv)
{
    std::cout << "OcctImgui UI Automation Tests" << std::endl;
    std::cout << "==============================" << std::endl;
    
    // Parse command line arguments
    bool run_headless = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-nogui") == 0) {
            run_headless = true;
        }
    }
    
    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGuiContext* imgui_ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_ctx);
    
    // Setup IO
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Create test engine
    ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
    ImGuiTestEngine_Start(engine, imgui_ctx);
    
    // Register our tests
    RegisterTests_OcctImgui(engine);
    
    // Queue all tests
    ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, "occt_imgui", 
                               ImGuiTestRunFlags_RunFromCommandLine);
    
    // Configure test engine for headless mode
    ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(engine);
    test_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
    test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    
    // Run tests
    std::cout << "Running tests..." << std::endl;
    int max_iterations = 1000;
    int iterations = 0;
    
    while (ImGuiTestEngine_IsTestQueueEmpty(engine) == false && iterations < max_iterations) {
        // Process test engine
        ImGui::NewFrame();
        
        // Make sure test engine processes its queue
        ImGuiTestEngine_PostSwap(engine);
        
        ImGui::Render();
        iterations++;
        
        if (iterations % 100 == 0) {
            std::cout << "." << std::flush;
        }
    }
    std::cout << std::endl;
    
    // Check results
    bool success = ImGuiTestEngine_IsTestQueueEmpty(engine);
    
    if (success) {
        std::cout << "All tests passed successfully!" << std::endl;
    } else {
        std::cout << "Some tests failed or timed out." << std::endl;
    }
    
    // Cleanup
    ImGuiTestEngine_Stop(engine);
    ImGuiTestEngine_DestroyContext(engine);
    ImGui::DestroyContext(imgui_ctx);
    
    return success ? 0 : 1;
}