#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "camera.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

constexpr unsigned int MAX_FRAMES = 2;

// object resources
struct MaterialResourcesBase {
    virtual ~MaterialResourcesBase() = default;
};

struct PBRResources : MaterialResourcesBase {
    AllocatedImage colorImage;
    VkSampler colorSampler;
    AllocatedImage metalRoughImage;
    VkSampler metalRoughSampler;
    VkBuffer dataBuffer;
    uint32_t dataBufferOffset;
};

struct EmitterResources : MaterialResourcesBase {
    VkBuffer dataBuffer;
    uint32_t dataBufferOffset;
};

struct OpenGLResources : MaterialResourcesBase {
    VkBuffer dataBuffer;
    uint32_t dataBufferOffset;
};

struct Material {
    MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;
	DescriptorWriter writer;

	virtual void buildPipelines(VulkanEngine* engine) = 0;
	virtual void clearResources(VkDevice device) = 0;

	virtual MaterialInstance writeMaterial(
        VkDevice device,
        MaterialPass pass,
        const MaterialResourcesBase& resources,
        DescriptorAllocator2& descriptorAllocator) = 0;
};

// object materials
struct GLTFMetallic_Roughness : Material {
	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[14];
	};

	virtual void buildPipelines(VulkanEngine* engine);
	virtual void clearResources(VkDevice device);

	virtual MaterialInstance writeMaterial(
        VkDevice device,
        MaterialPass pass,
        const MaterialResourcesBase& resources,
        DescriptorAllocator2& descriptorAllocator) override;
};

struct EmitterMaterial : Material {
	struct MaterialConstants {
		glm::vec4 colorFactors;
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[15];
	};

	virtual void buildPipelines(VulkanEngine* engine);
	virtual void clearResources(VkDevice device);

	virtual MaterialInstance writeMaterial(
        VkDevice device,
        MaterialPass pass,
        const MaterialResourcesBase& resources,
        DescriptorAllocator2& descriptorAllocator) override;
};

struct OpenGLMaterial : Material {
    struct MaterialConstants {
        glm::vec4 colorFactors;
        glm::vec4 ambient;
        glm::vec4 diffuse;
        glm::vec4 specular;
        float shininess;
        glm::vec4 extra[11];
    };

	virtual void buildPipelines(VulkanEngine* engine);
	virtual void clearResources(VkDevice device);

	virtual MaterialInstance writeMaterial(
        VkDevice device,
        MaterialPass pass,
        const MaterialResourcesBase& resources,
        DescriptorAllocator2& descriptorAllocator) override;
};

struct EmitterNode : public Node {
	std::shared_ptr<MeshAsset> mesh;

    AllocatedBuffer emitterConstants;
    EmitterMaterial::MaterialConstants* mappedConstants = nullptr;
    MaterialInstance materialInstance;
    VulkanEngine* engine;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;

    void setColor(const glm::vec4& color) {
        mappedConstants->colorFactors = color;
    }
};

struct MeshNode : public Node {
	std::shared_ptr<MeshAsset> mesh;

    AllocatedBuffer constants;
    void* mappedConstants = nullptr;
    MaterialInstance materialInstance;
    VulkanEngine* engine;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;
    Bounds bounds;
	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

// temporary structure that holds everything that'll be drawn in a single frame
struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
	std::vector<RenderObject> TransparentSurfaces;
};

struct EngineStats {
    float fps;
    float frametime;
    int triangle_count;
    int drawcall_count;
    float mesh_draw_time;
};

template<typename MaterialConstants, typename MaterialType, typename MaterialResources>
std::shared_ptr<MeshNode> createNode(std::shared_ptr<MeshAsset> mesh, MaterialType& materialType, MaterialResources& resources, VulkanEngine* engine);

class VulkanEngine {
public:
    bool _isInitialized { false };
    VkExtent2D _windowExtent{ 1700, 900 };
    int _rotation { 0 }; // imGui
    VkSampleCountFlagBits _maxSamples;
    

    // glfw window
    // struct GLFWwindow* _window { nullptr };
    struct SDL_Window* _window { nullptr };

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
    AllocatedImage _resolveImage;
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

    // pipelines
    VkPipeline _meshPipeline;
    VkPipelineLayout _meshPipelineLayout;

    // test meshes
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<MeshAsset>> emitterMeshes;
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> emitterMeshesNames;

    // texture images
    AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;

    VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

    VkDescriptorSetLayout _singleImageDescriptorLayout;

    // gltf
    EmitterMaterial emitterMaterial;
    OpenGLMaterial openglMaterial;
    MaterialInstance defaultData;
    GLTFMetallic_Roughness metalRoughMaterial;
    DrawContext mainDrawContext;
    std::unordered_map<std::string, std::shared_ptr<MeshNode>> loadedNodes;
    std::unordered_map<std::string, std::shared_ptr<MeshNode>> sceneNodes;
    std::unordered_map<std::string, std::shared_ptr<EmitterNode>> loadedEmitterNodes;
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
    
    // camera
    Camera mainCamera;

    // engine
    EngineStats stats;

    // imgui
    ImTextureRef _normalImageId;
    ImTextureRef _depthImageId;
    float sunlightDirectionX{ 0.0f };
    float sunlightDirectionY{ 1.0f };
    float sunlightDirectionZ{ 0.5f };

    float emitterPosX{ 0.0f };
    float emitterPosY{ -5.0f };
    float emitterPosZ{ 86.0f };

    void updateScene();

    // initializations
    void initVulkan();
    void initSwapchain();
    void createSwapchain(uint32_t width, uint32_t height);
    void createDrawImage(uint32_t width, uint32_t height);
    void createResolveImage(uint32_t width, uint32_t height);
    void initCommands();
    void initSyncStructures();
    void initDescriptors();
    void initPipelines();
    void initBackgroundPipelines();
    void initImgui();
    void initMeshPipeline();

    // helpers
    void drawBackground(VkCommandBuffer commandBuffer);
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void drawGeometry(VkCommandBuffer cmd);
    void initDefaultData(); // default vertex and index 
    void recreateSwapChain(); // for resizing
    bool is_visible(const RenderObject& obj, const glm::mat4& viewproj);
    std::shared_ptr<EmitterNode> createEmitterNode(std::shared_ptr<MeshAsset> mesh, const glm::vec4& initialColor);
    VkSampleCountFlagBits getMaxUsableSampleCount();

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
    bool freezeRendering{false};
    GPUMeshBuffers uploadMesh(std::vector<uint32_t> indices, std::vector<Vertex> vertices);
};