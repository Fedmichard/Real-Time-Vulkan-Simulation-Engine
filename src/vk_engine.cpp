#include "vk_engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include "vk_types.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "VkBootstrap.h"

#include <chrono>
#include <thread>

constexpr bool bUseValidationLayers = true;

VulkanEngine* loadedEngine = nullptr;

// declarations
static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

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
    initVulkan(); // init vulkan
    initSwapchain(); // init swapchain
    initCommands(); // init command pool and buffer
    initSyncStructures(); // init all our fences and semaphores
    initDescriptors(); // init all our descriptors
    initPipelines(); // init all our pipelines
    initImgui(); // init gui
    initDefaultData(); // maybe?

    mainCamera.velocity = glm::vec3(0.f);
    mainCamera.position = glm::vec3(0.0f, 0.0f, 5.0f);

    mainCamera.pitch = 0;
    mainCamera.yaw = 0;
    
    // everything was successful
    _isInitialized = true;
}

void VulkanEngine::cleanup() {
    if (_isInitialized) {
        vkDeviceWaitIdle(_device);

        for (int i = 0; i < MAX_FRAMES; i++) {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderFinishedSemaphore, nullptr);

            _frames[i]._deletionQueue.flush();
        }

        for (auto& mesh : testMeshes) {
            destroyBuffer(mesh->meshBuffers.indexBuffer);
            destroyBuffer(mesh->meshBuffers.vertexBuffer);
        }

        metalRoughMaterial.clearResources(_device);

        _mainDeletionQueue.flush();

        destroySwapchain();
        
        vkDestroySurfaceKHR(_instance, _surface, nullptr);

        vkDestroyDevice(_device, nullptr); 
        vkb::destroy_debug_utils_messenger(_instance, _debugMessenger);
        vkDestroyInstance(_instance, nullptr);
        
        glfwDestroyWindow(_window);

        glfwTerminate();
    }

    loadedEngine = nullptr;
}

void VulkanEngine::draw() {
    updateScene();
    // cpu will wait for fence to enter signaled state and then unsignal it (it will be signaled again once rendering is finished)
    VK_CHECK(vkWaitForFences(_device, 1, &getCurrentFrame()._renderFence, VK_TRUE, UINT64_MAX));
    getCurrentFrame()._deletionQueue.flush();
    getCurrentFrame()._frameDescriptors.clearPools(_device);

    // request next image in swap chain
    uint32_t swapchainImageIndex;
    VkResult result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, getCurrentFrame()._imageAvailableSemaphore, VK_NULL_HANDLE, &swapchainImageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resizeReuqested) {
        recreateSwapChain();
        return;
    }

	_drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * renderScale;
	_drawExtent.width= std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * renderScale;

    // select current frames command buffer and reset it for recording
    VkCommandBuffer cmd = getCurrentFrame()._mainCommandBuffer;

    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo = vkinit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    
    VK_CHECK(vkResetFences(_device, 1, &getCurrentFrame()._renderFence));

    // now we can record draw commands into buffer
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // transition the image into one that can be drawn to
    vkutil::transitionImageLayout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    drawBackground(cmd, _drawImage.image);

    // transition to draw geometry
    vkutil::transitionImageLayout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // transition depth attachment image
    vkutil::transitionImageLayout(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    drawGeometry(cmd);

    // transition the _drawImage.image for transfer src
    vkutil::transitionImageLayout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    // transition _swapchainImages into transfer dst
    vkutil::transitionImageLayout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // copy drawimage into swapchain image
    vkutil::copyImageToImage(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);
    // now that we copied from our draw image into swapchain image we transition it again so we can draw it correct format for imgui
    vkutil::transitionImageLayout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    drawImgui(cmd, _swapchainImageViews[swapchainImageIndex]);

    // now set swapchain image for presentation
    vkutil::transitionImageLayout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

    // now we must submit buffer to graphics queue for rendering
    /* abstract later */
    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = cmd;
    cmdInfo.deviceMask = 0;

    // signals that the image is now available for reading/writing
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

    // submit draw commands to graphics queue
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, getCurrentFrame()._renderFence));

    // prepare present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &getCurrentFrame()._renderFinishedSemaphore;
    presentInfo.pImageIndices = &swapchainImageIndex;

    // present
    result = vkQueuePresentKHR(_graphicsQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resizeReuqested) {
        recreateSwapChain();
        return;
    }

    _frameNumber++;
}

void VulkanEngine::run() {
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();

        mainCamera.processInput(_window);
        
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("background")) {
            ImGui::SliderFloat("Render Scale",&renderScale, 0.3f, 1.f);
			ComputeEffect& selected = backgroundEffects[currentBackgroundIndex];
		
			ImGui::Text("Selected effect: ", selected.name);
		
			ImGui::SliderInt("Effect Index", &currentBackgroundIndex, 0, backgroundEffects.size() - 1);
		
			ImGui::InputFloat4("data1",(float*)& selected.data.data1);
			ImGui::InputFloat4("data2",(float*)& selected.data.data2);
			ImGui::InputFloat4("data3",(float*)& selected.data.data3);
			ImGui::InputFloat4("data4",(float*)& selected.data.data4);
            
			ImGui::SliderInt("Model Rotation", &_rotation, 0, 1800);
		}

		ImGui::End();

        //make imgui calculate internal draw structures
        ImGui::Render();

        draw();
    }
}

/**********************************
*         Init Funcitons
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
}

void VulkanEngine::initSwapchain() {
    createSwapchain(_windowExtent.width, _windowExtent.height);

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
    imgInfo.extent = drawImageExtent;
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
    imgViewInfo.image = _drawImage.image;
    imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imgViewInfo.format = _drawImage.imageFormat;
    imgViewInfo.subresourceRange.baseMipLevel = 0;
    imgViewInfo.subresourceRange.levelCount = 1;
    imgViewInfo.subresourceRange.baseArrayLayer = 0;
    imgViewInfo.subresourceRange.layerCount = 1;
    imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    // create draw image view
    VK_CHECK(vkCreateImageView(_device, &imgViewInfo, nullptr, &_drawImage.imageView));

    // will use the same draw extent as draw image of course for our depth attachment
    _depthImage.imageExtent = drawImageExtent;
    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo depthInfo{};
    depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType = VK_IMAGE_TYPE_2D;
    depthInfo.format = _depthImage.imageFormat;
    depthInfo.extent = drawImageExtent;
    depthInfo.mipLevels = 1;
    depthInfo.arrayLayers = 1;
    depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.usage = depthImageUsages;

    vmaCreateImage(_allocator, &depthInfo, &imgAllocInfo, &_depthImage.image, &_depthImage.allocation, nullptr);

    VkImageViewCreateInfo depthImageView{};
    depthImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthImageView.image = _depthImage.image;
    depthImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthImageView.format = _depthImage.imageFormat;
    depthImageView.subresourceRange.baseMipLevel = 0;
    depthImageView.subresourceRange.levelCount = 1;
    depthImageView.subresourceRange.baseArrayLayer = 0;
    depthImageView.subresourceRange.layerCount = 1;
    depthImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    VK_CHECK(vkCreateImageView(_device, &depthImageView, nullptr, &_depthImage.imageView));

    // add to deletion queues
    _mainDeletionQueue.push_function([=]() {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

        vkDestroyImageView(_device, _depthImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
    });
}

void VulkanEngine::initCommands() {
    // command pool specifically for commands going into a graphics queue
    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows the command buffers to be reset and reused individually
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = _graphicsQueueFamily;

    // create each command buffer for each frame in flight
    for (int i = 0; i < MAX_FRAMES; i++) {
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = _frames[i]._commandPool;
        allocInfo.commandBufferCount = 1;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VK_CHECK(vkAllocateCommandBuffers(_device, &allocInfo, &_frames[i]._mainCommandBuffer));
    }

    // create command pool for immediate GPU commands
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immPool));

    VkCommandBufferAllocateInfo immCmdInfo{};
    immCmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    immCmdInfo.commandPool = _immPool;
    immCmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    immCmdInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(_device, &immCmdInfo, &_immBuffer));

    _mainDeletionQueue.push_function([&]() {
        vkDestroyCommandPool(_device, _immPool, nullptr);
    });
}

void VulkanEngine::initSyncStructures() {
    // for drawing
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start the fence signaled so we're not stuck waiting

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (int i = 0; i < MAX_FRAMES; i++) {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._imageAvailableSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderFinishedSemaphore));
    }

    // for imgui
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
    _mainDeletionQueue.push_function([&]() {
        vkDestroyFence(_device, _immFence, nullptr);
    });
}

void VulkanEngine::initDescriptors() {
    // create descriptor set layout
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    _drawImageDescriptorLayout = builder.build(_device);

    // scene data descriptor
    DescriptorLayoutBuilder sceneBuilder;
    sceneBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    _gpuSceneDataDescriptorLayout = sceneBuilder.build(_device);

    // texture descriptor
    DescriptorLayoutBuilder textureBuilder;
    textureBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    _singleImageDescriptorLayout = textureBuilder.build(_device);

    // create descriptor pool
    // pool size ratio holds a VkDescriptorType and a ratio of how many
    std::vector<DescriptorAllocator2::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    // initialize descriptor pool then allocate descriptor set from pool
    _descriptorAllocator.init(_device, 10, sizes);
    _drawImageDescriptorSet = _descriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

    // update descriptor set
    _descriptorWriter.writeImage(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    _descriptorWriter.updateSet(_device, _drawImageDescriptorSet);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _singleImageDescriptorLayout, nullptr);

        _descriptorAllocator.destroyPools(_device);
    });

    // dynamic descriptor allocation strat
    for (int i = 0; i < MAX_FRAMES; i++) {
		// create a descriptor pool
		std::vector<DescriptorAllocator2::PoolSizeRatio> frameSizes = { 
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		_frames[i]._frameDescriptors = DescriptorAllocator2{};
		_frames[i]._frameDescriptors.init(_device, 1000, frameSizes);
	
		_mainDeletionQueue.push_function([&, i]() {
			_frames[i]._frameDescriptors.destroyPools(_device);
		});
	}
}

void VulkanEngine::initPipelines() {
    initBackgroundPipelines();
    initMeshPipeline();
    metalRoughMaterial.buildPipelines(this);
}

void VulkanEngine::initImgui() {
    // 1. Create descriptor pool for IMGUI
    // the different descriptor pool types and how many (really big tbh)
    VkDescriptorPoolSize poolSizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &poolInfo, nullptr, &imguiPool));

    // initialize imgui library
    ImGui::CreateContext();

    // intializes for glfw
    ImGui_ImplGlfw_InitForVulkan(_window, true);

    // initializes for vulkan
    ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.Instance = _instance;
	initInfo.PhysicalDevice = _physicalDevice;
	initInfo.Device = _device;
	initInfo.Queue = _graphicsQueue;
	initInfo.DescriptorPool = imguiPool;
	initInfo.MinImageCount = 3;
	initInfo.ImageCount = 3;
	initInfo.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	initInfo.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;
	

	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&initInfo);

	// add the destroy the imgui created structures
	_mainDeletionQueue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
	});
}

/**********************************
*        Helper Functions
**********************************/

// initialize window
void VulkanEngine::initWindow(GLFWwindow** window, int width, int height) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    *window = glfwCreateWindow(width, height, "Vulkan Simulation Engine", nullptr, nullptr);
    // we do this because GLFW is a C library and doesn't handle the concept of this from classes
    // so we're passing a pointer of our HelloTriangleApplication class 
    glfwSetWindowUserPointer(*window, this);
    glfwSetFramebufferSizeCallback(*window, framebufferResizeCallback);

    glfwSetWindowUserPointer(*window, &mainCamera); 
    // Now, set the callback
    glfwSetCursorPosCallback(*window, Camera::mouse_callback);
}

// we're creating a static function as a callback because GLFW doesn't know how to properly call a member function with the correct "this" pointer 
// static members belong to the class themselves and not a specific instance
static void framebufferResizeCallback(GLFWwindow *window, int width, int height) {
    auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
    app->resizeReuqested = true;
}

// create surface
void VulkanEngine::createSurface(VkInstance& instance, GLFWwindow*& window, VkSurfaceKHR* surface) {
    // Glfw is creating a vulkan window surface linked to our glfw window
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

// create swap chain
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

// init background pipelines
void VulkanEngine::initBackgroundPipelines() {
    // global for all pipelines
    VkPipelineLayoutCreateInfo computePipelineLayoutInfo{};
    computePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computePipelineLayoutInfo.setLayoutCount = 1;
    computePipelineLayoutInfo.pSetLayouts = &_drawImageDescriptorLayout;

    // adding push constants
    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computePipelineLayoutInfo.pushConstantRangeCount = 1;
    computePipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    VK_CHECK(vkCreatePipelineLayout(_device, &computePipelineLayoutInfo, nullptr, &_gradientPipelineLayout));

    /* Gradient Shader */
    VkShaderModule gradientShader;
    if (!vkutil::loadShaderModule("../shaders/gradient.comp.spv", _device, &gradientShader)) {
        fmt::print("Error When building the compute shader \n");
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = gradientShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageInfo;

    ComputeEffect gradient;
    gradient.layout = _gradientPipelineLayout;
    gradient.name = "gradient";
    gradient.data = {};

    // default gradient data
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

    /* skybox shader */
    VkShaderModule skyShader;
    if (!vkutil::loadShaderModule("../shaders/sky.comp.spv", _device, &skyShader)) {
        fmt::print("Error when building the compute shader \n");
    }

    computePipelineCreateInfo.stage.module = skyShader;

    ComputeEffect sky;
    sky.layout = _gradientPipelineLayout;
    sky.name = "sky";
    sky.data = {};
    
    //default sky parameters
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4 ,0.97);

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));
    
    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(sky);

    vkDestroyShaderModule(_device, gradientShader, nullptr);
    vkDestroyShaderModule(_device, skyShader, nullptr);
    _mainDeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
        vkDestroyPipeline(_device, backgroundEffects[0].pipeline, nullptr);
        vkDestroyPipeline(_device, backgroundEffects[1].pipeline, nullptr);
    });
}

// draw our background
void VulkanEngine::drawBackground(VkCommandBuffer commandBuffer, VkImage image) {
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

    ComputeEffect& effect = backgroundEffects[currentBackgroundIndex];

    // bind the gradient drawing compute pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, backgroundEffects[currentBackgroundIndex].pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptorSet, 0, nullptr);

    vkCmdPushConstants(commandBuffer, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(commandBuffer, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}

// init pipeline for model loading
void VulkanEngine::initMeshPipeline() {
    VkPipelineLayoutCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    pipelineInfo.pushConstantRangeCount = 1;
    pipelineInfo.pPushConstantRanges = &bufferRange;
    pipelineInfo.setLayoutCount = 1;
    pipelineInfo.pSetLayouts = &_singleImageDescriptorLayout;
    
    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineInfo, nullptr, &_meshPipelineLayout));

    VkShaderModule triangleVertShader;
    if (!vkutil::loadShaderModule("../shaders/triangle_mesh.vert.spv", _device, &triangleVertShader)) {
        fmt::println("error when building the triangle fragment vertex module");
    } else {
        fmt::println("triangle vertex shader loaded successfully!");
    }

    VkShaderModule triangleFragShader;
    if (!vkutil::loadShaderModule("../shaders/text_image.frag.spv", _device, &triangleFragShader)) {
        fmt::println("error when building the triangle fragment vertex module");
    } else {
        fmt::println("triangle fragment shader loaded successfully!");
    }
    
    PipelineBuilder pipelineBuilder;
    //use the triangle layout we created
    pipelineBuilder._pipelineLayout = _meshPipelineLayout;
    //connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.setShaders(triangleVertShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.setMultisamplingNone();
	//no blending
	pipelineBuilder.disableBlending();
    // pipelineBuilder.enableBlendingAdditive();
    // pipelineBuilder.enableBlendingAlpha();
    // depth testing
	// pipelineBuilder.disableDepthtest();
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//connect the image format we will draw into, from draw image
	pipelineBuilder.setColorAttachmentFormat(_drawImage.imageFormat);
	pipelineBuilder.setDepthFormat(_depthImage.imageFormat);

	//finally build the pipeline
	_meshPipeline = pipelineBuilder.buildPipeline(_device);

	//clean structures
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertShader, nullptr);

	_mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _meshPipeline, nullptr);
	});
}

// init a default triangle
void VulkanEngine::initDefaultData() {
    // init data
    testMeshes = loadGltfMeshes(this,"..\\assets\\basicmesh.glb").value();

    // white image
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage = createImage((void*)&white, VkExtent3D{ 1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    // grey image
    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = createImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    // black image
	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	_blackImage = createImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    //checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 *16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	_errorCheckerboardImage = createImage(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    // nearest and linear samplers
    VkSamplerCreateInfo sampler{};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = VK_FILTER_NEAREST;
    sampler.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(_device, &sampler, nullptr, &_defaultSamplerNearest);

    sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_device, &sampler, nullptr, &_defaultSamplerLinear);

	_mainDeletionQueue.push_function([&](){
		vkDestroySampler(_device,_defaultSamplerNearest,nullptr);
		vkDestroySampler(_device,_defaultSamplerLinear,nullptr);

		destroyImage(_whiteImage);
		destroyImage(_greyImage);
		destroyImage(_blackImage);
		destroyImage(_errorCheckerboardImage);
	});

    GLTFMetallic_Roughness::MaterialResources materialResources;
	//default the material textures
	materialResources.colorImage = _whiteImage;
	materialResources.colorSampler = _defaultSamplerLinear;
	materialResources.metalRoughImage = _whiteImage;
	materialResources.metalRoughSampler = _defaultSamplerLinear;

	//set the uniform buffer for the material data
	AllocatedBuffer materialConstants = createBuffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//write the buffer
	GLTFMetallic_Roughness::MaterialConstants* sceneUniformData = (GLTFMetallic_Roughness::MaterialConstants*)materialConstants.allocation->GetMappedData();
	sceneUniformData->colorFactors = glm::vec4{1,1,1,1};
	sceneUniformData->metal_rough_factors = glm::vec4{1,0.5,0,0};

	_mainDeletionQueue.push_function([=]() {
		destroyBuffer(materialConstants);
	});

	materialResources.dataBuffer = materialConstants.buffer;
	materialResources.dataBufferOffset = 0;

	defaultData = metalRoughMaterial.writeMaterial(_device,MaterialPass::MainColor,materialResources, _descriptorAllocator);

    for (auto& m : testMeshes) {
		std::shared_ptr<MeshNode> newNode = std::make_shared<MeshNode>();
		newNode->mesh = m;

		newNode->localTransform = glm::mat4{ 1.f };
		newNode->worldTransform = glm::mat4{ 1.f };

		for (auto& s : newNode->mesh->surfaces) {
			s.material = std::make_shared<GLTFMaterial>();
			s.material->data = defaultData;
		}

		loadedNodes[m->name] = std::move(newNode);
	}
}

void VulkanEngine::drawGeometry(VkCommandBuffer cmd) {
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = _drawImage.imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = _depthImage.imageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil.depth = 1.0f;

	VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = VkRect2D { VkOffset2D { 0, 0 }, _drawExtent };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;
    renderInfo.pDepthAttachment = &depthAttachment;

    // allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = createBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	getCurrentFrame()._deletionQueue.push_function([=]() {
		destroyBuffer(gpuSceneDataBuffer);
    });

	//write the buffer
	GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
	*sceneUniformData = sceneData;

    // begin dynamic rendering
	vkCmdBeginRendering(cmd, &renderInfo);
        //set dynamic viewport and scissor
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = _drawExtent.width;
        viewport.height = _drawExtent.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = _drawExtent.width;
        scissor.extent.height = _drawExtent.height;

        vkCmdSetScissor(cmd, 0, 1, &scissor);
        // pipeline for vertex buffer draws
        // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);
        for (const RenderObject& draw : mainDrawContext.OpaqueSurfaces) {
            vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->pipeline);

            // bind a texture
            VkDescriptorSet globalDescriptor = getCurrentFrame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);
            DescriptorWriter writer;
            writer.writeBuffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            writer.updateSet(_device, globalDescriptor);

            vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,draw.material->pipeline->layout, 0,1, &globalDescriptor,0,nullptr );
            vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,draw.material->pipeline->layout, 1,1, &draw.material->materialSet,0,nullptr );

            vkCmdBindIndexBuffer(cmd, draw.indexBuffer,0,VK_INDEX_TYPE_UINT32);

            GPUDrawPushConstants pushConstants;
            pushConstants.vertexBuffer = draw.vertexBufferAddress;
            pushConstants.worldMatrix = draw.transform;
            vkCmdPushConstants(cmd,draw.material->pipeline->layout ,VK_SHADER_STAGE_VERTEX_BIT,0, sizeof(GPUDrawPushConstants), &pushConstants);

            vkCmdDrawIndexed(cmd,draw.indexCount,1,draw.firstIndex,0,0);
        }
	vkCmdEndRendering(cmd);
}

// UI
void VulkanEngine::drawImgui(VkCommandBuffer cmd, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = targetImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = nullptr ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0}, _swapchainExtent };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

// quick submit to a command buffer
void VulkanEngine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) {
    // we don't use semaphores since we're not synchronizing with swapchain (which requires 2 queue families)
    VK_CHECK(vkResetFences(_device, 1, &_immFence)); // reset the fence into the unsignaled state immediately since we signal it upon create
    VK_CHECK(vkResetCommandBuffer(_immBuffer, 0));

    VkCommandBuffer cmd = _immBuffer;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = cmd;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdInfo;

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, _immFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, VK_TRUE, UINT64_MAX));
}



AllocatedBuffer VulkanEngine::createBuffer(size_t allocSize, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = allocSize;
    bufferInfo.usage = bufferUsage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaallocInfo{};
    vmaallocInfo.usage = memoryUsage; // helps vma decide what type of memory is needed. CPU-GPU; CPU ONLY; GPU ONLY; GPU-CPU;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // maps memory to buffer

    AllocatedBuffer newBuffer;

    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

	return newBuffer;
}

AllocatedImage VulkanEngine::createImage(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped) {
	AllocatedImage newImage;
	newImage.imageFormat = format;
	newImage.imageExtent = extent;

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = format;
    imgInfo.extent = extent;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = usage;

	if (mipmapped) {
		imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocinfo{};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &imgInfo, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct aspect flag
	VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT) {
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = newImage.image;
    viewInfo.format = format;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    // what aspect of an image you want to access (color depth stencil metadata)
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;
    viewInfo.subresourceRange.aspectMask = aspectMask; // we want to expose the color aspect of the image

	// build a image-view for the image
	VK_CHECK(vkCreateImageView(_device, &viewInfo, nullptr, &newImage.imageView));

	return newImage;
}

AllocatedImage VulkanEngine::createImage(void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped) {
	size_t dataSize = extent.depth * extent.width * extent.height * 4;

	AllocatedBuffer uploadbuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	memcpy(uploadbuffer.info.pMappedData, data, dataSize);

	AllocatedImage newImage = createImage(extent, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

	immediateSubmit([&](VkCommandBuffer cmd) {
		vkutil::transitionImageLayout(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = extent;

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			&copyRegion);

		vkutil::transitionImageLayout(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});

	destroyBuffer(uploadbuffer);

	return newImage;
}

void VulkanEngine::destroyImage(const AllocatedImage& img) {
    vkDestroyImageView(_device, img.imageView, nullptr);
    vmaDestroyImage(_allocator, img.image, img.allocation);
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::vector<uint32_t> indices, std::vector<Vertex> vertices) {
	const size_t vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	const size_t indexBufferSize = sizeof(indices[0]) * indices.size();

    // holds mesh resources
	GPUMeshBuffers newSurface;
    
    // create staging buffer that holds vertex and index buffer
    AllocatedBuffer staging = createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	//create vertex buffer
	newSurface.vertexBuffer = createBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	//find the adress of the vertex buffer
	VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressInfo);

	//create index buffer
	newSurface.indexBuffer = createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

    void* data = staging.allocation->GetMappedData(); // pointer to memory location on gpu

    // copy from host to memory location
    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize); // FIX: Add offset here

    // copy from staging buffer to newsurface mesh buffers (index + vertex)
    immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{};
        vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize; // we stored everything in staging buffer so add offset of vertex buffer to get index
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    destroyBuffer(staging);

    return newSurface;
}

void VulkanEngine::recreateSwapChain() {
    vkDeviceWaitIdle(_device);

    // make sure the old versions of these objets are cleaned up before recreation
    destroySwapchain();

    int width, height;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }

    _windowExtent.width = width;
    _windowExtent.height = height;

    // swap chain will have to be recreated because that is our queue of images
    createSwapchain(_windowExtent.width, _windowExtent.height);
    
    resizeReuqested = false;
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

void VulkanEngine::destroyBuffer(const AllocatedBuffer& buffer) {
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

/******************************************************************************************************
*                                              Others
******************************************************************************************************/

void GLTFMetallic_Roughness::buildPipelines(VulkanEngine* engine) {
	VkPipelineLayoutCreateInfo meshLayoutInfo{};
    meshLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    materialLayout = layoutBuilder.build(engine->_device);
    
	VkDescriptorSetLayout layouts[] = { engine->_gpuSceneDataDescriptorLayout, materialLayout };

	meshLayoutInfo.pushConstantRangeCount = 1;
	meshLayoutInfo.pPushConstantRanges = &matrixRange;
	meshLayoutInfo.setLayoutCount = 2;
	meshLayoutInfo.pSetLayouts = layouts;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &meshLayoutInfo, nullptr, &newLayout));

	VkShaderModule meshVertexShader;
	if (!vkutil::loadShaderModule("../shaders/mesh.vert.spv", engine->_device, &meshVertexShader)) {
		fmt::println("Error when building the triangle vertex shader module");
	}

    VkShaderModule meshFragShader;
	if (!vkutil::loadShaderModule("../shaders/mesh.frag.spv", engine->_device, &meshFragShader)) {
		fmt::println("Error when building the triangle fragment shader module");
	}
    

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

	// build the stage-create-info for both vertex and fragment stages. This lets
	// the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder._pipelineLayout = newLayout;
	pipelineBuilder.setShaders(meshVertexShader, meshFragShader);
	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.setMultisamplingNone();
	pipelineBuilder.disableBlending();
	pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_LESS);

	//render format
	pipelineBuilder.setColorAttachmentFormat(engine->_drawImage.imageFormat);
	pipelineBuilder.setDepthFormat(engine->_depthImage.imageFormat);


	// finally build the pipeline
    opaquePipeline.pipeline = pipelineBuilder.buildPipeline(engine->_device);

	// create the transparent variant
	pipelineBuilder.enableBlendingAdditive();

	pipelineBuilder.enableDepthTest(false, VK_COMPARE_OP_LESS);

	transparentPipeline.pipeline = pipelineBuilder.buildPipeline(engine->_device);
	
	vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
	vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);
}

void GLTFMetallic_Roughness::clearResources(VkDevice device) {
	vkDestroyDescriptorSetLayout(device,materialLayout,nullptr);
	vkDestroyPipelineLayout(device,transparentPipeline.layout,nullptr);

	vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
	vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocator2& descriptorAllocator)
{
	MaterialInstance matData;
	matData.passType = pass;
	if (pass == MaterialPass::Transparent) {
		matData.pipeline = &transparentPipeline;
	}
	else {
		matData.pipeline = &opaquePipeline;
	}

	matData.materialSet = descriptorAllocator.allocate(device, materialLayout);


	writer.clear();
	writer.writeBuffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.writeImage(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.writeImage(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.updateSet(device, matData.materialSet);

	return matData;
}

void MeshNode::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
	glm::mat4 nodeMatrix = topMatrix * worldTransform;

	for (auto& s : mesh->surfaces) {
		RenderObject def;
		def.indexCount = s.count;
		def.firstIndex = s.startIndex;
		def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
		def.material = &s.material->data;

		def.transform = nodeMatrix;
		def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;
		
		ctx.OpaqueSurfaces.push_back(def);
	}

	// recurse down
	Node::Draw(topMatrix, ctx);
}

void VulkanEngine::updateScene() {
	mainDrawContext.OpaqueSurfaces.clear();
    mainCamera.update();

    glm::mat4 view = mainCamera.getViewMatrix();
    glm::mat4 proj = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 0.1f, 10000.0f);

    proj[1][1] *= -1;

	// sceneData.view = glm::translate(glm::vec3{ 0,0,-5 });
	// camera projection
	sceneData.proj = proj;
    sceneData.view = view;

    glm::mat4 model = glm::rotate(glm::radians(1.0f) * _rotation, glm::vec3(0.0f, 1.0f, 0.0f));

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	loadedNodes["Suzanne"]->Draw(glm::mat4{1.f}, mainDrawContext);	
    
	sceneData.viewproj = sceneData.proj * sceneData.view * model;

	//some default lighting parameters
	sceneData.ambientColor = glm::vec4(.1f);
	sceneData.sunlightColor = glm::vec4(1.f);
	sceneData.sunlightDirection = glm::vec4(0,1,0.5,1.f);
}