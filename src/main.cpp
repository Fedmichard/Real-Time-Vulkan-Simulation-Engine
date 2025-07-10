#include "vk_engine.h"

int main(int argc, char* argv[]) {
    VulkanEngine engine;

    // initialize window and vulkan
    engine.init();

    // main loop
    engine.run();

    // uninitialize and free memory
    engine.cleanup();

    return 0;
}