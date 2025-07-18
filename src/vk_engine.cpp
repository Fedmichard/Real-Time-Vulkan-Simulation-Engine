#include "vk_engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

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
    initVulkan(); // init vulkan
    initSwapchain(); // init swapchain
    initCommands(); // init command pool and buffer
    initSyncStructures(); // init all our fences and semaphores
    initDescriptors(); // init all our descriptors
    initPipelines(); // init all our pipelines
    initImgui(); // init gui
    
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

    _drawExtent.width = _drawImage.imageExtent.width;
    _drawExtent.height = _drawImage.imageExtent.height;

    // now we can record draw commands into buffer
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // transition the image into one that can be drawn to
    vkutil::transitionImageLayout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    drawBackground(cmd, _drawImage.image);

    // transition the _drawImage.image for transfer src
    vkutil::transitionImageLayout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    // transition _swapchainImages into transfer dst
    vkutil::transitionImageLayout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // copy drawimage into swapchain image
    vkutil::copyImageToImage(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);
    // now that we copied from our draw image into swapchain image we transition it again so we can draw it correct format for imgui
    vkutil::transitionImageLayout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // draw imgui onto swapchain image
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
    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    _frameNumber++;
}

void VulkanEngine::run() {
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("background")) {
			
			ComputeEffect& selected = backgroundEffects[currentBackgroundIndex];
		
			ImGui::Text("Selected effect: ", selected.name);
		
			ImGui::SliderInt("Effect Index", &currentBackgroundIndex, 0, backgroundEffects.size() - 1);
		
			ImGui::InputFloat4("data1",(float*)& selected.data.data1);
			ImGui::InputFloat4("data2",(float*)& selected.data.data2);
			ImGui::InputFloat4("data3",(float*)& selected.data.data3);
			ImGui::InputFloat4("data4",(float*)& selected.data.data4);
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

void VulkanEngine::initSwapchain() {
    createSwapchain(_windowExtent.width, _windowExtent.height);
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

    // create descriptor pool
    // pool size ratio holds a VkDescriptorType and a ratio of how many
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };

    _globalDescriptorAllocator.initPool(_device, 10, sizes); // preallocate 10 max sets for 10 different images

    // allocate descriptor set
    _drawImageDescriptorSet = _globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

    // create descriptors within the set
    // binding 0
    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = _drawImage.imageView;

    VkWriteDescriptorSet drawImageWrite{};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = _drawImageDescriptorSet;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);

        _globalDescriptorAllocator.destroyPool(_device);
    });
}

void VulkanEngine::initPipelines() {
    initBackgroundPipelines();
}

// create background gradient pipeline
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

    // different gradients
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
        vkDestroyPipeline(_device, gradient.pipeline, nullptr);
        vkDestroyPipeline(_device, sky.pipeline, nullptr);
    });
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

/**********************************
*          Deallocation
**********************************/

void VulkanEngine::destroySwapchain() {
    for (const auto& view : _swapchainImageViews) {
        vkDestroyImageView(_device, view, nullptr);
    }

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}