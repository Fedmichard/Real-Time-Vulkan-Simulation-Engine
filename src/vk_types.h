// will add our main resuable types here

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <deque>
#include <span>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "vk_descriptors.h"

// these next 35 lines are for generating a scene graph
struct DrawContext;

// base class for a renderable dynamic object
class IRenderable {
    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : public IRenderable {

    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refreshTransform(worldTransform);
        }
    }

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx)
    {
        // draw children
        for (auto& c : children) {
            c->Draw(topMatrix, ctx);
        }
    }
};

enum class MaterialPass :uint8_t {
    MainColor,
    Transparent,
    Other
};

struct MaterialPipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

// holds scene data
struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

// compute push constants
struct ComputePushConstants {
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct ComputeEffect {
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;
};

// struct that holds everything we need for an allocated image
struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation; // VkDeviceMemory probably
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

// struct that holds everything we need for an allocated buffer
struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation; // VkDeviceMemory probably
    VmaAllocationInfo info;
};

// for meshes
struct Vertex {
	glm::vec3 position;
	float uvX;
	glm::vec3 normal;
	float uvY;
	glm::vec4 color;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct DeletionQueue  {
    /*
        double ended queue, meaning you can add to the front and back but we're only using back
        wrapper around a function, lambda expression, or functor object that takes no args and returns void
        a queue of functions, lambda expression, etc that will be invoked later
    */
	std::deque<std::function<void()>> deletors;

    // the struct holds a function that will push a function onto the queue
    // && is an rvalue reference
	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

    // flush invokes every functor in the queue in reverse order
	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct FrameData {
    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
    VkSemaphore _imageAvailableSemaphore, _renderFinishedSemaphore;
    VkFence _renderFence;
    DeletionQueue _deletionQueue;
    // to allocate descriptor sets at runtime, we will hold one descriptor allocator in our FrameData structure
    DescriptorAllocator2 _frameDescriptors;
};


#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)