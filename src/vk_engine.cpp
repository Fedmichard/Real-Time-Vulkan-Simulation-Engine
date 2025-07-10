#include "vk_engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vk_types.h"
#include "vk_initializers.h"
#include "VkBootstrap.h"

#include <chrono>
#include <thread>


constexpr bool bUseValidationLayers = true;

VulkanEngine* loadedEngine = nullptr;

// declarations
void initWindow(GLFWwindow** window, int width, int height);
void createSurface(VkInstance& instance, GLFWwindow*& window, VkSurfaceKHR* surface);

/**********************************
*             Engine
**********************************/

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init() {
    // can only initialize one engine per app
    assert(loadedEngine == nullptr);
    loadedEngine = this;
    // GLFW window initialization
    initWindow(&_window, _windowExtent.width, _windowExtent.height);

    // initialize vulkan
    initVulkan();
    initSwapchain();
    initCommands();
    initSyncStructures();
    
    // everything was successful
    _isInitialized = true;
}

void VulkanEngine::cleanup() {
    if (_isInitialized) {
        vkDeviceWaitIdle(_device);

        destroySwapchain();

        vkDestroyDevice(_device, nullptr); 
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkb::destroy_debug_utils_messenger(_instance, _debugMessenger);
        vkDestroyInstance(_instance, nullptr);
        
        glfwDestroyWindow(_window);

        glfwTerminate();
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
*         Engine Functions
**********************************/

void VulkanEngine::initVulkan() {
    vkb::InstanceBuilder builder;

    // use vkbootstrap to create the vulkan instance
    auto instBuilder = builder.set_app_name("Real-Time Vulkan Simulation Engine")
        .request_validation_layers(bUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    vkb::Instance instance = instBuilder.value();

    _instance = instance.instance;
    _debugMessenger = instance.debug_messenger;

    // create the surface
    createSurface(_instance, _window, &_surface);

    // vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    // vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    // use vkbootstrap to select a physical device
    vkb::PhysicalDeviceSelector selector { instance };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(_surface)
        .select()
        .value();

    // create logical device
    vkb::DeviceBuilder deviceBuilder { physicalDevice };
    vkb::Device device = deviceBuilder.build().value();

    _device = device.device;
    _physicalDevice = physicalDevice.physical_device;
}

void VulkanEngine::createSwapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder { _physicalDevice, _device, _surface };

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain swapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    _swapchainExtent = swapchain.extent;
    _swapchain = swapchain.swapchain;
    _swapchainImages = swapchain.get_images().value();
    _swapchainImageViews = swapchain.get_image_views().value();
}

void VulkanEngine::initSwapchain() {
    createSwapchain(_windowExtent.width, _windowExtent.height);
}

void VulkanEngine::initCommands() {
    
}

void VulkanEngine::initSyncStructures() {
    
}

/**********************************
*        Helper Functions
**********************************/

// initialize window
void initWindow(GLFWwindow** window, int width, int height) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    *window = glfwCreateWindow(width, height, "Vulkan Simulation Engine", nullptr, nullptr);
}

// create surface
void createSurface(VkInstance& instance, GLFWwindow*& window, VkSurfaceKHR* surface) {
    // Glfw is creating a vulkan window surface linked to our glfw window
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

/**********************************
*          Deallocation
**********************************/

void VulkanEngine::destroySwapchain() {
    for (const auto& view : _swapchainImageViews) {
        vkDestroyImageView(_device, view, nullptr);
    }

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}