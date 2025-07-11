#pragma once

#include "vk_types.h"

struct FrameData {
    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
    VkSemaphore _imageAvailableSemaphore, _renderFinishedSemaphore;
    VkFence _renderFence;
};

constexpr unsigned int MAX_FRAMES = 2;

class VulkanEngine {
private:
    bool _isInitialized { false };
    int _frameNumber { 0 };
    bool _stopRendering { false };
    VkExtent2D _windowExtent{ 1700, 900 };

    // glfw window
    struct GLFWwindow* _window { nullptr };

    static VulkanEngine& Get();

    // member variables
    FrameData _frames[MAX_FRAMES];

    // queues
    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    // member functions
    FrameData& getCurrentFrame() { return _frames[_frameNumber % MAX_FRAMES]; };

    // vulkan handles
    VkInstance _instance;
	VkDebugUtilsMessengerEXT _debugMessenger;
	VkPhysicalDevice _physicalDevice;
	VkDevice _device;
	VkSurfaceKHR _surface;

    VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

    // initializations
    void initVulkan();
    void initSwapchain();
    void createSwapchain(uint32_t width, uint32_t height);
    void initCommands();
    void initSyncStructures();

    // cleanup
    void destroySwapchain();

public:
    // initializes engine
    void init();

    // shuts down engine and frees memory
    void cleanup();

    // draw loop
    void draw();

    // main loop
    void run();
};