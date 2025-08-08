#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "camera.h"

constexpr unsigned int MAX_FRAMES = 2;

struct GLTFMetallic_Roughness {
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	void buildPipelines(VulkanEngine* engine);
	void clearResources(VkDevice device);

	MaterialInstance writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocator2& descriptorAllocator);
};

struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;

	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};
/* START HERE WHEN I GETG BACK*/
// temporary structure that holds everything that'll be drawn in a single frame
struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
};

class VulkanEngine {
public:
    bool _isInitialized { false };
    VkExtent2D _windowExtent{ 1700, 900 };
    int _rotation { 0 }; // imGui
    

    // glfw window
    struct GLFWwindow* _window { nullptr };

    static VulkanEngine& Get();

    // sync structures
    int _frameNumber { 0 };
    FrameData _frames[MAX_FRAMES];
    FrameData& getCurrentFrame() { return _frames[_frameNumber % MAX_FRAMES]; };

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

    // gltf
    MaterialInstance defaultData;
    GLTFMetallic_Roughness metalRoughMaterial;
    DrawContext mainDrawContext;
    std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

    // camera
    Camera mainCamera;

    void updateScene();

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
    void drawBackground(VkCommandBuffer commandBuffer);
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