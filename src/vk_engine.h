#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"

constexpr unsigned int MAX_FRAMES = 2;

class VulkanEngine {
private:
    bool _isInitialized { false };
    int _frameNumber { 0 };
    VkExtent2D _windowExtent{ 1700, 900 };

    // glfw window
    struct GLFWwindow* _window { nullptr };

    static VulkanEngine& Get();

    // member variables
    FrameData _frames[MAX_FRAMES];

    // queues
    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

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
    AllocatedImage _depthImage;
    VkExtent2D _drawExtent;
    float renderScale = 1.0f;

    // compute pipeline layout
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

    // draw pipeline
    VkPipeline _trianglePipeline;
    VkPipelineLayout _trianglePipelineLayout;

    // member functions
    FrameData& getCurrentFrame() { return _frames[_frameNumber % MAX_FRAMES]; };
    AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage);

    // mesh pipeline
    VkPipeline _meshPipeline;
    VkPipelineLayout _meshPipelineLayout;

    // rectangle mesh
    GPUMeshBuffers _rectangleMesh;

    // test meshes
    std::vector<std::shared_ptr<MeshAsset>> testMeshes;

    // imGui
    int _rotation { 180 };

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
    void initMeshPipeline();

    // helpers
    void initWindow(GLFWwindow** window, int width, int height);
    void createSurface(VkInstance& instance, GLFWwindow*& window, VkSurfaceKHR* surface);
    void drawBackground(VkCommandBuffer commandBuffer, VkImage image);
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void drawGeometry(VkCommandBuffer cmd);
    void initDefaultData(); // default vertex and index 
    void recreateSwapChain(); // for resizing

    // cleanup
    void destroySwapchain();
    void destroyBuffer(const AllocatedBuffer& buffer);

public:
    // initializes engine
    void init();

    // shuts down engine and frees memory
    void cleanup();

    // draw loop
    void draw();

    // main loop
    void run();

    // public functions needed
    bool resizeReuqested { false };
    GPUMeshBuffers uploadMesh(std::vector<uint32_t> indices, std::vector<Vertex> vertices);
};