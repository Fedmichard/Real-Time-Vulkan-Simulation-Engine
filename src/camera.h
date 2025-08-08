#include "vk_types.h"
#include <GLFW/glfw3.h>

class Camera {
public:
    glm::vec3 velocity;
    glm::vec3 position;
    // vertical rotation
    float pitch { 0.f };
    // horizontal rotation
    float yaw { 0.f };

    glm::mat4 getViewMatrix();
    glm::mat4 getRotationMatrix();

    void update();
    void processInput(GLFWwindow* window);
};