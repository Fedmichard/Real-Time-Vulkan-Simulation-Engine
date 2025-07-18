#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"

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

    // vulkan initial handles
    VkInstance _instance;
	VkDebugUtilsMessengerEXT _debugMessenger;
	VkPhysicalDevice _physicalDevice;
	VkDevice _device;
	VkSurfaceKHR _surface;

    // swap chain
    VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

    // free memory
    DeletionQueue _mainDeletionQueue;

    // memory allocator for buffers and images
    VmaAllocator _allocator;
    AllocatedImage _drawImage;
    VkExtent2D _drawExtent;

    // compute pipeline
    VkPipeline _gradientPipeline;
    VkPipelineLayout _gradientPipelineLayout;

    // allocate descriptor sets
    DescriptorAllocator _globalDescriptorAllocator;
    VkDescriptorSet _drawImageDescriptorSet;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    // immediate submit structures
    VkFence _immFence;
    VkCommandPool _immPool;
    VkCommandBuffer _immBuffer;

    // array of compute pipelines we will be drawing and the push constants (data1, data2,...)
    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundIndex{0};

    // initializations
    void initVulkan();
    void initSwapchain();
    void createSwapchain(uint32_t width, uint32_t height);
    void initCommands();
    void initSyncStructures();
    void initDescriptors();
    void initPipelines();
    void initBackgroundPipelines();
    void initImgui();

    // helpers
    void drawBackground(VkCommandBuffer commandBuffer, VkImage image);
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);

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