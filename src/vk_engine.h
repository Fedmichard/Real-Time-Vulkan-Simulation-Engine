#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "camera.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RIGHT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

constexpr unsigned int MAX_FRAMES = 2;

// object resources
struct MaterialResourcesBase {
    VkBuffer dataBuffer;
    uint32_t dataBufferOffset;
};

struct PBRResources : MaterialResourcesBase {
    AllocatedImage colorImage;
    VkSampler colorSampler;
    AllocatedImage metalRoughImage;
    VkSampler metalRoughSampler;
};

struct EmitterResources : MaterialResourcesBase {};

struct OpenGLResources : MaterialResourcesBase {
    AllocatedImage texture;
    VkSampler sampler;
    AllocatedImage metalTexture;
    VkSampler metalSampler;
};

struct DepthResources : MaterialResourcesBase {};

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
        float constant;
        float linear;
        float quadratic;
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

struct OpenGLMaterial : Material {
    struct MaterialConstants {
        glm::vec4 colorFactors;
        float shininess;
        glm::vec4 extra[13];
    };

	virtual void buildPipelines(VulkanEngine* engine);
	virtual void clearResources(VkDevice device);

	virtual MaterialInstance writeMaterial(
        VkDevice device,
        MaterialPass pass,
        const MaterialResourcesBase& resources,
        DescriptorAllocator2& descriptorAllocator) override;
};

struct DepthMaterial : Material {
    struct MaterialConstants {};

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
    glm::vec4 direction; // direction
    glm::vec4 position; // w light type 0 = point, 1 = spot, 2 = directional
    glm::mat4 positionMatrix; // local object matrix
    float cutOff;
    float innerCutOff;
    float outerCutOff;
    MaterialInstance materialInstance;
    VulkanEngine* engine;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;

    void changePosition(const glm::vec3& pos) {
        position = glm::vec4{pos.x, pos.y, pos.z, position.w};
        positionMatrix = glm::translate(glm::mat4{1.f}, glm::vec3{position.x, position.y, position.z});
    }

    void setColor(const glm::vec3& color) {
        mappedConstants->colorFactors = glm::vec4{color.x, color.y, color.z, mappedConstants->colorFactors.w};
    }

    void setIntensity(const float& intensity) {
        mappedConstants->colorFactors.w = intensity;
    }

    void setDirection(const glm::vec3& direction) {
        this->direction = glm::vec4{direction, this->direction.w};
    }

    void setCutOff(const float& cutOff) {
        this->cutOff = glm::cos(glm::radians(cutOff));
    }
};

struct ObjectNode : public Node {
	std::shared_ptr<MeshAsset> mesh;

    AllocatedBuffer constants;
    void* mappedConstants = nullptr;
    MaterialInstance materialInstance;
    VulkanEngine* engine;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
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
std::shared_ptr<ObjectNode> createNode(std::shared_ptr<MeshAsset> mesh,
    const char* name,
    MaterialType& materialType,
    MaterialResources& resources,
    VulkanEngine* engine);

class VulkanEngine {
public:
    static VulkanEngine& Get();

    bool _isInitialized { false };
    VkExtent2D _windowExtent{ 1700, 900 };
    VkSampleCountFlagBits _maxSamples;
    // window
    struct SDL_Window* _window { nullptr };

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
    VkExtent2D _drawExtent;
    VmaAllocator _allocator;
    AllocatedImage _drawImage;
    AllocatedImage _resolveImage;
    AllocatedImage _depthImage;
    AllocatedImage _depthViewImage;
    AllocatedImage _depthViewDepthImage;
    float renderScale = 1.0f;

    // compute pipeline layout
    VkPipelineLayout _gradientPipelineLayout;

    // allocate descriptor sets
    DescriptorAllocator2 _descriptorAllocator;
    DescriptorWriter _descriptorWriter;
    VkDescriptorSet _drawImageDescriptorSet;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    // scene data
    GPUSceneData sceneData;
    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

    // immediate submit structures
    VkFence _immFence;
    VkCommandPool _immPool;
    VkCommandBuffer _immBuffer;

    // array of compute pipelines we will be drawing and the push constants (data1, data2,...)
    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundIndex{0};

    // pipelines
    VkPipeline _meshPipeline;
    VkPipelineLayout _meshPipelineLayout;

    // scene meshes
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshesNames;

    // default texture images
    AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;
    // default texture samplers
    VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

    VkDescriptorSetLayout _singleImageDescriptorLayout;

    EmitterMaterial emitterMaterial;
    OpenGLMaterial openglMaterial;
    GLTFMetallic_Roughness metalRoughMaterial;
    DepthMaterial depthMaterial;

    // gltf
    DrawContext mainDrawContext;
    std::unordered_map<std::string, std::shared_ptr<ObjectNode>> sceneNodes;
    std::unordered_map<std::string, std::shared_ptr<EmitterNode>> emitterNodes;
    std::vector<std::shared_ptr<EmitterNode>> sceneLights;
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
    
    // camera
    Camera mainCamera;

    // engine
    EngineStats stats;

    // imgui
    ImTextureRef _normalImageId;
    ImTextureRef _depthImageId;
    float sunlightDirectionX{ -0.2f };
    float sunlightDirectionY{ -1.0f };
    float sunlightDirectionZ{ -0.3f };

    float emitterPosX{ 0.0f };
    float emitterPosY{ -5.0f };
    float emitterPosZ{ 12.5f };

    void updateScene();

    // initializations
    void initVulkan();
    void initSwapchain();
    void createSwapchain(uint32_t width, uint32_t height);
    void createDrawImage(uint32_t width, uint32_t height);
    void createResolveImage(uint32_t width, uint32_t height);
    void createDepthViewImage(uint32_t width, uint16_t height);
    void initCommands();
    void initSyncStructures();
    void initDescriptors();
    void initPipelines();
    void initBackgroundPipelines();
    void initImgui();
    void initMeshPipeline();

    // helpers
    AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage);
    // create images
    AllocatedImage createImage(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage createImage(void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    void destroyImage(const AllocatedImage& img);
    void drawBackground(VkCommandBuffer commandBuffer);
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void drawGeometry(VkCommandBuffer cmd);
    void drawDepthImage(VkCommandBuffer cmd);
    void initDefaultData(); // default vertex and index 
    void recreateSwapChain(); // for resizing
    bool is_visible(const RenderObject& obj, const glm::mat4& viewproj);
    std::shared_ptr<EmitterNode> createEmitterNode(std::shared_ptr<MeshAsset> mesh, const char* name);
    std::shared_ptr<EmitterNode> createEmitterNode(std::shared_ptr<MeshAsset> mesh,
        const char* name,
        const glm::vec4& position,
        const glm::vec4& initialColor);
    std::shared_ptr<EmitterNode> createEmitterNode(std::shared_ptr<MeshAsset> mesh,
        const char* name,
        const glm::vec4& position,
        const glm::vec4& initialColor,
        const glm::vec4& direction,
        const float cutOff);
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