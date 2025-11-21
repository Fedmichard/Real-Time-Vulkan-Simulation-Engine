#include "camera.h"
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RIGHT_HANDED
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

glm::mat4 Camera::getViewMatrix() const
{
    // to create a correct model view, we need to move the world in the opposite 
    // direction to the camera
    // so we will create the camera model matrix and invert
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix() const
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into
    // the final rotation matrix

    glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3 { 1.f, 0.f, 0.f });
    glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3 { 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

glm::vec3 Camera::getForwardDirection() const
{
    // Get the rotation matrix you already have
    glm::mat4 cameraRotation = getRotationMatrix();

    // The forward vector is the negative of the Z-axis vector, which is the third column of the matrix
    // In GLM, columns are accessed with [2]
    return -glm::vec3(cameraRotation[2]);
}

void Camera::processSDLEvent(SDL_Event& e)
{
    glm::mat4 cameraRotation = getRotationMatrix();
    
    const float camSpeed = 0.0125f;
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_w) { velocity.z = -camSpeed; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = camSpeed; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = -camSpeed; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = camSpeed; }
        if (e.key.keysym.sym == SDLK_LCTRL) { velocity.y = -camSpeed; }
        if (e.key.keysym.sym == SDLK_SPACE) { velocity.y = camSpeed; }
    }

    if (e.type == SDL_KEYUP) {
        if (e.key.keysym.sym == SDLK_w) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_LCTRL) { velocity.y = 0; }
        if (e.key.keysym.sym == SDLK_SPACE) { velocity.y = 0; }
    }

    if (e.type == SDL_MOUSEMOTION) {
        yaw += (float)e.motion.xrel / 200.f;
        pitch -= (float)e.motion.yrel / 200.f;
    }
}

void Camera::update()
{
    /*
    glm::mat4 cameraRotation = getRotationMatrix();

    glm::vec3 moveVector(0.0f);

    glm::vec3 forwardRaw = glm::vec3(0, 0, 1);
    glm::vec3 upRaw = glm::vec3(0, 1, 0);
    glm::vec3 rightRaw = glm::vec3(1, 0, 0);

    glm::vec3 forward = glm::vec3(cameraRotation * glm::vec4(forwardRaw, 0.0f));
    glm::vec3 up = glm::vec3(cameraRotation * glm::vec4(upRaw, 0.0f));
    glm::vec3 right = glm::vec3(cameraRotation * glm::vec4(rightRaw, 0.0f));

    moveVector += forward * velocity.z;
    moveVector += right * velocity.x;
    moveVector.y += velocity.y;

    position += moveVector * 0.5f;
    */
    
    glm::mat4 cameraRotation = getRotationMatrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}