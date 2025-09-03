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
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "../../client/occt/OcctRenderClient.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <cstdio>

// GLFW error callback function (can reuse from OcctRenderClient or define a new one)
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return EXIT_FAILURE;
    }

    // Set GLFW window hints (similar to those previously in OcctRenderClient::initWindow)
    const bool toAskCoreProfile = true;
    if (toAskCoreProfile) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#if defined(__APPLE__)
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }
    // glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE); // Optional
    // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // Optional


    // Create GLFW window
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "OCCT ImGui App", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);
    // Enable VSync (optional)
    glfwSwapInterval(1); 

    // Create OcctRenderClient instance, passing the GLFW window
    OcctRenderClient anApp(window);

    try
    {
        // Show window before run (previously myOcctWindow->Map() inside OcctRenderClient)
        glfwShowWindow(window); 
        anApp.run();
    }
    catch (const std::runtime_error& theError)
    {
        std::cerr << "Runtime Error: " << theError.what() << std::endl;
        // OcctRenderClient::cleanup() no longer handles GLFW window destruction or termination
    }
    catch (...) // Catch other potential exceptions
    {
        std::cerr << "An unknown error occurred." << std::endl;
    }
    
    // OcctRenderClient::cleanup() will handle ImGui and OCCT related resource release
    // But GLFW window destruction and GLFW termination are now controlled by main

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
