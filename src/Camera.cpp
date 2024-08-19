#include "Camera.h"

#include "Error.h"
#include "Helpers.h"

using namespace Mandrill;

namespace
{
    const float rotationSpeed = 1.0f;
    const float zoomSpeed = 10.0f;
    const float minZoom = 0.1f;
    const float maxZoom = 150.0f;
    const float mouseSpeed = 0.0008f;
} // namespace


Camera::Camera(std::shared_ptr<Device> pDevice, GLFWwindow* pWindow) : mpDevice(pDevice), mpWindow(pWindow)
{
    mPosition = glm::vec3(1.0f, 1.0f, 1.0f);
    setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    mUp = glm::vec3(0.0f, 1.0f, 0.0f);

    int width, height;
    glfwGetWindowSize(mpWindow, &width, &height);
    mAspect = static_cast<float>(width) / static_cast<float>(height);

    mFov = 30.0f;
    mNear = 0.01f;
    mFar = 1000.0f;
    mMoveSpeed = 1.0f;

    mpUniforms = std::make_shared<Buffer>(mpDevice, 4 * sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    mBufferInfo.buffer = mpUniforms->getBuffer();
    mBufferInfo.offset = 0;
    mBufferInfo.range = 4 * sizeof(glm::mat4);
}

Camera::~Camera()
{
}

void Camera::update(float delta, glm::vec2 cursorDelta)
{
    // Right vector, used for pitch rotations and strafing
    glm::vec3 right = glm::normalize(glm::cross(mDirection, mUp));

    // Speed control with L-Shift and L-Ctrl
    float speedFactor = 1.0f;
    if (glfwGetKey(mpWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        speedFactor = 2.5f;
    }
    if (glfwGetKey(mpWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        speedFactor = 0.1f;
    }

    // Move and strafe with WASD
    if (glfwGetKey(mpWindow, GLFW_KEY_W) == GLFW_PRESS) {
        mPosition += speedFactor * mMoveSpeed * delta * mDirection;
    }

    if (glfwGetKey(mpWindow, GLFW_KEY_S) == GLFW_PRESS) {
        mPosition -= speedFactor * mMoveSpeed * delta * mDirection;
    }

    if (glfwGetKey(mpWindow, GLFW_KEY_A) == GLFW_PRESS) {
        mPosition += speedFactor * mMoveSpeed * delta * right;
    }

    if (glfwGetKey(mpWindow, GLFW_KEY_D) == GLFW_PRESS) {
        mPosition -= speedFactor * mMoveSpeed * delta * right;
    }

    // Up and down with Q and E
    if (glfwGetKey(mpWindow, GLFW_KEY_E) == GLFW_PRESS) {
        mPosition += speedFactor * mMoveSpeed * delta * mUp;
    }

    if (glfwGetKey(mpWindow, GLFW_KEY_Q) == GLFW_PRESS) {
        mPosition -= speedFactor * mMoveSpeed * delta * mUp;
    }

    // Direction with arrows
    glm::vec3 newDir(1.0f);
    const float angle = speedFactor * rotationSpeed * delta;
    if (glfwGetKey(mpWindow, GLFW_KEY_UP) == GLFW_PRESS) {
        newDir = glm::rotate(mDirection, angle, right);
    }

    if (glfwGetKey(mpWindow, GLFW_KEY_DOWN) == GLFW_PRESS) {
        newDir = glm::rotate(mDirection, -angle, right);
    }

    if (glfwGetKey(mpWindow, GLFW_KEY_LEFT) == GLFW_PRESS) {
        newDir = glm::rotate(mDirection, -angle, mUp);
    }

    if (glfwGetKey(mpWindow, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        newDir = glm::rotate(mDirection, angle, mUp);
    }

    // Zoom with PERIOD and COMMA
    if (glfwGetKey(mpWindow, GLFW_KEY_PERIOD) == GLFW_PRESS) {
        mFov -= speedFactor * zoomSpeed * delta;
        mFov = std::clamp(mFov, minZoom, maxZoom);
    }

    if (glfwGetKey(mpWindow, GLFW_KEY_COMMA) == GLFW_PRESS) {
        mFov += speedFactor * zoomSpeed * delta;
        mFov = std::clamp(mFov, minZoom, maxZoom);
    }

    // Mouse control
    if ((cursorDelta.x != 0.0f || cursorDelta.y != 0.0f) &&
        (glfwGetMouseButton(mpWindow, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS || mMouseCaptured)) {
        mDirection = glm::rotate(mDirection, cursorDelta.x * mouseSpeed, mUp);
        newDir = glm::normalize(glm::rotate(mDirection, -cursorDelta.y * mouseSpeed, right));
    }

    // Clamp vertical direction change
    if (fabsf(newDir.y) < 0.99f) {
        mDirection = newDir;
    }
}

VkWriteDescriptorSet Camera::getDescriptor(uint32_t binding) const
{
    glm::mat4 view = glm::lookAt(mPosition, mPosition + mDirection, mUp);
    glm::mat4 viewInv = glm::inverse(view);
    glm::mat4 proj = glm::perspective(glm::radians(mFov), mAspect, mNear, mFar);

    proj[1][1] *= -1.0f; // GLM and Vulkan are not using the same coordinate system

    glm::mat4 projInv = glm::inverse(proj);

    std::array<glm::mat4, 4> matrices{view, viewInv, proj, projInv};

    mpUniforms->copyFromHost(matrices.data(), matrices.size() * sizeof(glm::mat4));

    VkWriteDescriptorSet descriptor = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &mBufferInfo,
    };

    return descriptor;
}