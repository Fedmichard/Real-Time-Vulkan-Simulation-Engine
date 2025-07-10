#include "vk_engine.h"

#include <ostream>
#include <iostream>

int main(int argc, char* argv[]) {
    VulkanEngine engine;

    try {
        // initialize window and vulkan
        engine.init();

        // main loop
        engine.run();

        // uninitialize and free memory
        engine.cleanup();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}