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
};


#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)