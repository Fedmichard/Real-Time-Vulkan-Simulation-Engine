#include "vk_engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vk_types.h"
#include "vk_initializers.h"

#include <chrono>
#include <thread>


constexpr bool bUseValidationLayers = false;

VulkanEngine* loadedEngine = nullptr;

// declarations
void initWindow(GLFWwindow** window, int width, int height);

/**********************************
*          Engine Code
**********************************/

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init() {
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // GLFW window initialization
    initWindow(&_window, _windowExtent.width, _windowExtent.height);

    // vulkan initialization
    _isInitialized = true;

}

void VulkanEngine::cleanup() {
    if (_isInitialized) {
        glfwDestroyWindow(_window);
    }

    loadedEngine = nullptr;
}

void VulkanEngine::draw() {

}

void VulkanEngine::run() {
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        draw();
    }
}

/**********************************
*       Helper Functions
**********************************/

// initialize window
void initWindow(GLFWwindow** window, int width, int height) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    *window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
}