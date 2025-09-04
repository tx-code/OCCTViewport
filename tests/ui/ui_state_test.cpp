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

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

// UI State logic for pure thin client
class UIControlState {
private:
    bool is_connected_;
    
public:
    UIControlState() : is_connected_(false) {}
    
    void SetConnectionState(bool connected) {
        is_connected_ = connected;
        spdlog::info("Connection state changed to: {}", connected ? "connected" : "disconnected");
    }
    
    bool IsConnected() const {
        return is_connected_;
    }
    
    // Pure thin client: all controls are disabled when disconnected
    bool IsControlEnabled(const std::string& control_name) const {
        if (!is_connected_) {
            return false; // All controls disabled when disconnected
        }
        return true; // All controls enabled when connected
    }
    
    // Simulate button click - only works when enabled
    bool HandleButtonClick(const std::string& button_name) {
        if (!IsControlEnabled(button_name)) {
            spdlog::warn("Button '{}' is disabled - cannot click", button_name);
            return false;
        }
        
        spdlog::info("Button '{}' clicked successfully", button_name);
        return true;
    }
};

// Test fixture
class UIStateTest : public ::testing::Test {
protected:
    UIControlState ui_state_;
    
    void SetUp() override {
        spdlog::set_level(spdlog::level::info);
    }
};

// Test that all buttons are disabled when disconnected
TEST_F(UIStateTest, ButtonsDisabledWhenDisconnected) {
    // Set disconnected state
    ui_state_.SetConnectionState(false);
    
    // Test various buttons
    EXPECT_FALSE(ui_state_.IsControlEnabled("Create Box"));
    EXPECT_FALSE(ui_state_.IsControlEnabled("Create Cone"));
    EXPECT_FALSE(ui_state_.IsControlEnabled("Create Sphere"));
    EXPECT_FALSE(ui_state_.IsControlEnabled("Create Cylinder"));
    EXPECT_FALSE(ui_state_.IsControlEnabled("Create Demo Scene"));
    EXPECT_FALSE(ui_state_.IsControlEnabled("Clear All"));
    EXPECT_FALSE(ui_state_.IsControlEnabled("Refresh Meshes"));
    
    // Verify clicks don't work
    EXPECT_FALSE(ui_state_.HandleButtonClick("Create Box"));
    EXPECT_FALSE(ui_state_.HandleButtonClick("Clear All"));
}

// Test that all buttons are enabled when connected
TEST_F(UIStateTest, ButtonsEnabledWhenConnected) {
    // Set connected state
    ui_state_.SetConnectionState(true);
    
    // Test various buttons
    EXPECT_TRUE(ui_state_.IsControlEnabled("Create Box"));
    EXPECT_TRUE(ui_state_.IsControlEnabled("Create Cone"));
    EXPECT_TRUE(ui_state_.IsControlEnabled("Create Sphere"));
    EXPECT_TRUE(ui_state_.IsControlEnabled("Create Cylinder"));
    EXPECT_TRUE(ui_state_.IsControlEnabled("Create Demo Scene"));
    EXPECT_TRUE(ui_state_.IsControlEnabled("Clear All"));
    EXPECT_TRUE(ui_state_.IsControlEnabled("Refresh Meshes"));
    
    // Verify clicks work
    EXPECT_TRUE(ui_state_.HandleButtonClick("Create Box"));
    EXPECT_TRUE(ui_state_.HandleButtonClick("Clear All"));
}

// Test state transitions
TEST_F(UIStateTest, StateTransitions) {
    // Start disconnected
    ui_state_.SetConnectionState(false);
    EXPECT_FALSE(ui_state_.IsConnected());
    EXPECT_FALSE(ui_state_.IsControlEnabled("Create Box"));
    
    // Transition to connected
    ui_state_.SetConnectionState(true);
    EXPECT_TRUE(ui_state_.IsConnected());
    EXPECT_TRUE(ui_state_.IsControlEnabled("Create Box"));
    EXPECT_TRUE(ui_state_.HandleButtonClick("Create Box"));
    
    // Transition back to disconnected
    ui_state_.SetConnectionState(false);
    EXPECT_FALSE(ui_state_.IsConnected());
    EXPECT_FALSE(ui_state_.IsControlEnabled("Create Box"));
    EXPECT_FALSE(ui_state_.HandleButtonClick("Create Box"));
}

// Test pure thin client behavior
TEST_F(UIStateTest, PureThinClientBehavior) {
    // Pure thin client: no local operations when disconnected
    ui_state_.SetConnectionState(false);
    
    // Verify no controls are available
    std::vector<std::string> all_controls = {
        "Create Box", "Create Cone", "Create Sphere", "Create Cylinder",
        "Create Demo Scene", "Clear All", "Refresh Meshes"
    };
    
    for (const auto& control : all_controls) {
        EXPECT_FALSE(ui_state_.IsControlEnabled(control)) 
            << "Control '" << control << "' should be disabled in pure thin client when disconnected";
        EXPECT_FALSE(ui_state_.HandleButtonClick(control))
            << "Button '" << control << "' should not be clickable in pure thin client when disconnected";
    }
    
    // Connect to server
    ui_state_.SetConnectionState(true);
    
    // Now all controls should work
    for (const auto& control : all_controls) {
        EXPECT_TRUE(ui_state_.IsControlEnabled(control))
            << "Control '" << control << "' should be enabled when connected";
        EXPECT_TRUE(ui_state_.HandleButtonClick(control))
            << "Button '" << control << "' should be clickable when connected";
    }
}

// Test rapid connection state changes
TEST_F(UIStateTest, RapidConnectionChanges) {
    // Simulate rapid connection/disconnection
    for (int i = 0; i < 10; ++i) {
        bool connected = (i % 2 == 0);
        ui_state_.SetConnectionState(connected);
        
        // Verify state is consistent
        EXPECT_EQ(ui_state_.IsConnected(), connected);
        EXPECT_EQ(ui_state_.IsControlEnabled("Create Box"), connected);
        EXPECT_EQ(ui_state_.HandleButtonClick("Create Box"), connected);
    }
}