#include "vk_engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "vk_types.h"
#include "vk_initializers.h"
#include "vk_images.h"
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

        _mainDeletionQueue.flush();
        for (int i = 0; i < MAX_FRAMES; i++) {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderFinishedSemaphore, nullptr);

            _frames[i]._deletionQueue.flush();
        }

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
    // cpu will wait for fence to enter signaled state and then unsignal it (it will be signaled again once rendering is finished)
    VK_CHECK(vkWaitForFences(_device, 1, &getCurrentFrame()._renderFence, VK_TRUE, UINT64_MAX));
    getCurrentFrame()._deletionQueue.flush();
    VK_CHECK(vkResetFences(_device, 1, &getCurrentFrame()._renderFence));

    // request next image in swap chain
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, getCurrentFrame()._imageAvailableSemaphore, VK_NULL_HANDLE, &swapchainImageIndex));

    // select current frames command buffer and reset it for recording
    VkCommandBuffer cmd = getCurrentFrame()._mainCommandBuffer;

    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo = vkinit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // now we can record draw commands into buffer
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // transition the image into one that can be drawn to
    vkutil::transitionImageLayout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    drawBackground(cmd);

    // transition the _drawImage.image for transfer src
    vkutil::transitionImageLayout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    // transition _swapchainImages into transfer dst
    vkutil::transitionImageLayout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // copy drawimage into swapchain image
    vkutil::copyImageToImage(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);
    // now set swapchain image for presentation
    vkutil::transitionImageLayout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

    // now we must submit buffer to graphics queue for rendering
    /* abstract later */
    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = cmd;
    cmdInfo.deviceMask = 0;

    // signals that the image is now available for reading/writing (this is what we check before we start drawing so now the image is available again)
    /* abstract later */
    VkSemaphoreSubmitInfo imageAvailableSemaphore{};
    imageAvailableSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    imageAvailableSemaphore.semaphore = getCurrentFrame()._imageAvailableSemaphore;
    imageAvailableSemaphore.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    imageAvailableSemaphore.deviceIndex = 0;
    imageAvailableSemaphore.value = 1;
    
    // signals image has finished being rendered to and can proceed to presentation
    /* abstract later */
    VkSemaphoreSubmitInfo renderFinishedSemaphore{};
    renderFinishedSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    renderFinishedSemaphore.semaphore = getCurrentFrame()._renderFinishedSemaphore;
    renderFinishedSemaphore.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    renderFinishedSemaphore.deviceIndex = 0;
    renderFinishedSemaphore.value = 1;
    
    VkSubmitInfo2 submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.waitSemaphoreInfoCount = 1;
    submit.pWaitSemaphoreInfos = &imageAvailableSemaphore;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &renderFinishedSemaphore;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, getCurrentFrame()._renderFence));

    // prepare present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &getCurrentFrame()._renderFinishedSemaphore;
    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    _frameNumber++;
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
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    // vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;

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

    _graphicsQueue = device.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = device.get_queue_index(vkb::QueueType::graphics).value();

    // create memory allocator
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = _physicalDevice;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    // destroy allocator
    _mainDeletionQueue.push_function([&]() {
        vmaDestroyAllocator(_allocator);
    });

    // creating an image that will use a much higher precision format that we will draw to and then transfer to swap chain with low latency
    VkExtent3D drawImageExtent;
    drawImageExtent.width = _windowExtent.width;
    drawImageExtent.height = _windowExtent.height;
    drawImageExtent.depth = 1;

    // hard coding draw format to VK_FORMAT_R16G16B16A16_SFLOAT from VK_FORMAT_B8G8R8A8_UNORM
    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT; // for compute shaders
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // draw geometry onto it

    // image create info
    /* abstract later */
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = _drawImage.imageFormat;
    imgInfo.extent = _drawImage.imageExtent;
    imgInfo.mipLevels = 1; // no mipmapping yet
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT; // no multisampling yet
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL; // used for optimal gpu reading
    imgInfo.usage = drawImageUsages;

    // allocation info for gpu
    VmaAllocationCreateInfo imgAllocInfo{};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY; 
    imgAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // create the image and allocate it on the gpu 
    vmaCreateImage(_allocator, &imgInfo, &imgAllocInfo, &_drawImage.image, &_drawImage.allocation, nullptr);

    // image view create info
    /* abstract later */
    VkImageViewCreateInfo imgViewInfo{};
    imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imgViewInfo.image = _drawImage.image;
    imgViewInfo.format = _drawImage.imageFormat;
    imgViewInfo.subresourceRange.baseMipLevel = 0;
    imgViewInfo.subresourceRange.levelCount = 1;
    imgViewInfo.subresourceRange.baseArrayLayer = 0;
    imgViewInfo.subresourceRange.layerCount = 1;
    imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    // create image view
    VK_CHECK(vkCreateImageView(_device, &imgViewInfo, nullptr, &_drawImage.imageView)); 

    // add to deletion queues
    _mainDeletionQueue.push_function([=]() {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
    });
}

void VulkanEngine::createSwapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder { _physicalDevice, _device, _surface };

    // will need format again later for other render targets, graphics pipeline, and render pass
    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    // swap chain with desired format, present mode, and extent
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
    // command pool specifically for commands going into a graphics queue
    VkCommandPoolCreateInfo commandPool{};
    commandPool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows the command buffers to be reset and reused individually
    commandPool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPool.queueFamilyIndex = _graphicsQueueFamily;

    // create each command buffer for each frame in flight
    for (int i = 0; i < MAX_FRAMES; i++) {
        VK_CHECK(vkCreateCommandPool(_device, &commandPool, nullptr, &_frames[i]._commandPool));

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = _frames[i]._commandPool;
        allocInfo.commandBufferCount = 1;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VK_CHECK(vkAllocateCommandBuffers(_device, &allocInfo, &_frames[i]._mainCommandBuffer));
    }
}

void VulkanEngine::initSyncStructures() {
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // start the fence signaled so we're not stuck waiting
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (int i = 0; i < MAX_FRAMES; i++) {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._imageAvailableSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderFinishedSemaphore));
    }
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

void VulkanEngine::drawBackground(VkCommandBuffer commandBuffer) {
    // clear the background
    VkClearColorValue clearValue;
    float flash = std::abs(std::sin(_frameNumber / 120.0f));
    clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

    /* abstract later */
    VkImageSubresourceRange clearRange;
    clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    clearRange.baseArrayLayer = 0;
    clearRange.levelCount = VK_REMAINING_MIP_LEVELS;
    clearRange.baseMipLevel = 0;

    // clear the image first (acts as background later)
    vkCmdClearColorImage(commandBuffer, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
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