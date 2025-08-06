#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"

constexpr unsigned int MAX_FRAMES = 2;

// Material is a data set that uses textures and other data to define how a surface looks
struct GLTFMetallicRoughness {
	VkDescriptorSetLayout materialLayout; // layout for the descriptor set
	DescriptorWriter writer;

	MaterialPipeline opaquePipeline; // one for opaque textures
	MaterialPipeline transparentPipeline; // one for transparent textures

    // uniform buffer 
	struct MaterialConstants {
		glm::vec4 colorFactors; // multiply the color texture
		glm::vec4 metalRoughFactors;
		//padding, we need it anyway for uniform buffers (uniform buffer needs to be a minimum of 256 bytes)
		glm::vec4 extra[14]; // vec4 = 16 bites (4 bits per x, y, z, a)
	};

    // descriptor set will hold some textures we want to bind
	struct MaterialResources {
        // color image texture binding 1 of ds
		AllocatedImage colorImage;
		VkSampler colorSampler;
        // metal texture binding 2 of ds
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
        // uniform buffer that holds material constants binding 0 of ds
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	void buildPipelines(VulkanEngine* engine);
	void clearResources(VkDevice device);

    // creates descriptor set and return a material instance struct that will be used for rendering
	MaterialInstance writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocator2& descriptorAllocator);
};

class VulkanEngine {
public:
    bool _isInitialized { false };
    int _frameNumber { 0 };
    VkExtent2D _windowExtent{ 1700, 900 };
    int _rotation { 180 }; // imGui
    

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
    DescriptorAllocator2 _descriptorAllocator; // NEW
    DescriptorWriter _descriptorWriter; // NEW
    VkDescriptorSet _drawImageDescriptorSet;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    // scene data
    GPUSceneData sceneData;

    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

    // immediate submit structures
    VkFence _immFence;
    VkCommandPool _immPool;
    VkCommandBuffer _immBuffer;

    // member functions
    FrameData& getCurrentFrame() { return _frames[_frameNumber % MAX_FRAMES]; };
    AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage);
    // create images
    AllocatedImage createImage(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage createImage(void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    void destroyImage(const AllocatedImage& img);

    // array of compute pipelines we will be drawing and the push constants (data1, data2,...)
    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundIndex{0};

    // mesh pipeline
    VkPipeline _meshPipeline;
    VkPipelineLayout _meshPipelineLayout;

    // test meshes
    std::vector<std::shared_ptr<MeshAsset>> testMeshes;

    // texture images
    AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;

    VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

    VkDescriptorSetLayout _singleImageDescriptorLayout;

    // gltf loading
    MaterialInstance defaultData;
    GLTFMetallicRoughness metalRoughMaterial;

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