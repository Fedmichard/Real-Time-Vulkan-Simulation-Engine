#pragma once

#include "vk_types.h"

class VulkanEngine {
private:
    bool _isInitialized { false };
    int _frameNumber { 0 };
    bool _stopRendering { false };
    VkExtent2D _windowExtent{ 1700, 900 };

    struct GLFWwindow* _window { nullptr };

public:
    static VulkanEngine& Get();

    // initializes engine
    void init();

    // shuts down engine and frees memory
    void cleanup();

    // draw loop
    void draw();

    // main loop
    void run();
};