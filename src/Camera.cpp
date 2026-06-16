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


Camera::Camera(ptr<Device> pDevice, uint32_t frameInFlightCount) : mpDevice(pDevice)
{
    mPosition = glm::vec3(1.0f, 1.0f, 1.0f);
    setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    mUp = glm::vec3(0.0f, 1.0f, 0.0f);

    mAspectRatio = 16.0f / 9.0f;
    mFov = 30.0f;
    mNear = 0.01f;
    mFar = 1000.0f;
    mOrthoSize = 10.0f;
    mMoveSpeed = 1.0f;

    mpUniforms = std::make_shared<Buffer>(mpDevice, frameInFlightCount * sizeof(CameraMatrices),
                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

Camera::~Camera()
{
    if (mDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(mpDevice->getDevice(), mDescriptorSetLayout, nullptr);
    }

    if (mRayTracingDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(mpDevice->getDevice(), mRayTracingDescriptorSetLayout, nullptr);
    }
}

void Camera::createDescriptor(VkShaderStageFlags stageFlags)
{
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount = 1,
        .stageFlags = stageFlags,
    };

    VkDescriptorSetLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };

    Check::Vk(vkCreateDescriptorSetLayout(mpDevice->getDevice(), &ci, nullptr, &mDescriptorSetLayout));

    std::vector<DescriptorDesc> desc;
    desc.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, mpUniforms, 0, sizeof(CameraMatrices));
    mpDescriptor = mpDevice->createDescriptor(desc, mDescriptorSetLayout);
}

void Camera::createRayTracingDescriptor(VkShaderStageFlags stageFlags)
{
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount = 1,
        .stageFlags = stageFlags,
    };

    VkDescriptorSetLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };

    Check::Vk(vkCreateDescriptorSetLayout(mpDevice->getDevice(), &ci, nullptr, &mRayTracingDescriptorSetLayout));

    std::vector<DescriptorDesc> desc;
    desc.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, mpUniforms, 0, sizeof(CameraMatrices));
    mpRayTracingDescriptor = mpDevice->createDescriptor(desc, mRayTracingDescriptorSetLayout);
}

void Camera::update(uint32_t frameInFlightIndex)
{
    // Update uniform buffer
    CameraMatrices* matrices = static_cast<CameraMatrices*>(mpUniforms->getHostMap()) + frameInFlightIndex;
    matrices->view = glm::lookAt(mPosition, mPosition + mDirection, mUp);
    matrices->view_inv = glm::inverse(matrices->view);
    matrices->proj = mProjection;
    matrices->proj[1][1] *= -1.0f; // GLM and Vulkan are not using the same coordinate system
    matrices->proj_inv = glm::inverse(matrices->proj);
}

void Camera::update(GLFWwindow* pWindow, float delta, glm::vec2 cursorDelta, uint32_t frameInFlightIndex)
{
    // Right vector, used for pitch rotations and strafing
    glm::vec3 right = glm::normalize(glm::cross(mDirection, mUp));

    // Speed control with L-Shift and L-Ctrl
    float speedFactor = 1.0f;
    if (glfwGetKey(pWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        speedFactor = 2.5f;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        speedFactor = 0.1f;
    }

    // Move and strafe with WASD
    if (glfwGetKey(pWindow, GLFW_KEY_W) == GLFW_PRESS) {
        mPosition += speedFactor * mMoveSpeed * delta * mDirection;
    }

    if (glfwGetKey(pWindow, GLFW_KEY_S) == GLFW_PRESS) {
        mPosition -= speedFactor * mMoveSpeed * delta * mDirection;
    }

    if (glfwGetKey(pWindow, GLFW_KEY_A) == GLFW_PRESS) {
        mPosition -= speedFactor * mMoveSpeed * delta * right;
    }

    if (glfwGetKey(pWindow, GLFW_KEY_D) == GLFW_PRESS) {
        mPosition += speedFactor * mMoveSpeed * delta * right;
    }

    // Up and down with Q and E
    if (glfwGetKey(pWindow, GLFW_KEY_E) == GLFW_PRESS) {
        mPosition += speedFactor * mMoveSpeed * delta * mUp;
    }

    if (glfwGetKey(pWindow, GLFW_KEY_Q) == GLFW_PRESS) {
        mPosition -= speedFactor * mMoveSpeed * delta * mUp;
    }

    // Direction with arrows
    glm::vec3 newDir(1.0f);
    const float angle = speedFactor * rotationSpeed * delta;
    if (glfwGetKey(pWindow, GLFW_KEY_UP) == GLFW_PRESS) {
        newDir = glm::rotate(mDirection, angle, right);
    }

    if (glfwGetKey(pWindow, GLFW_KEY_DOWN) == GLFW_PRESS) {
        newDir = glm::rotate(mDirection, -angle, right);
    }

    if (glfwGetKey(pWindow, GLFW_KEY_LEFT) == GLFW_PRESS) {
        newDir = glm::rotate(mDirection, angle, mUp);
    }

    if (glfwGetKey(pWindow, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        newDir = glm::rotate(mDirection, -angle, mUp);
    }

    // Zoom with PERIOD and COMMA
    if (glfwGetKey(pWindow, GLFW_KEY_PERIOD) == GLFW_PRESS) {
        mFov -= speedFactor * zoomSpeed * delta;
        mFov = std::clamp(mFov, minZoom, maxZoom);
    }

    if (glfwGetKey(pWindow, GLFW_KEY_COMMA) == GLFW_PRESS) {
        mFov += speedFactor * zoomSpeed * delta;
        mFov = std::clamp(mFov, minZoom, maxZoom);
    }

    // Mouse control
    if ((cursorDelta.x != 0.0f || cursorDelta.y != 0.0f) &&
        (glfwGetMouseButton(pWindow, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS || mMouseCaptured)) {
        mDirection = glm::rotate(mDirection, -cursorDelta.x * mouseSpeed, mUp);
        newDir = glm::normalize(glm::rotate(mDirection, -cursorDelta.y * mouseSpeed, right));
    }

    // Clamp vertical direction change
    if (fabsf(newDir.y) < 0.99f) {
        mDirection = newDir;
    }

    // Update uniform buffer
    CameraMatrices* matrices = static_cast<CameraMatrices*>(mpUniforms->getHostMap()) + frameInFlightIndex;
    matrices->view = glm::lookAt(mPosition, mPosition + mDirection, mUp);
    matrices->view_inv = glm::inverse(matrices->view);
    matrices->proj = mProjection;
    matrices->proj[1][1] *= -1.0f; // GLM and Vulkan are not using the same coordinate system
    matrices->proj_inv = glm::inverse(matrices->proj);
}

void Camera::setProjection(const CameraProjection projectionType)
{
    mProjectionType = projectionType;
}

static glm::vec4 getRow(const glm::mat4& m, int row)
{
    return glm::vec4(m[0][row], m[1][row], m[2][row], m[3][row]);
}

static glm::vec4 normalizePlane(const glm::vec4& p)
{
    float len = glm::length(glm::vec3(p));
    return p / len;
}

Frustum Camera::getFrustum(uint32_t frameInFlightIndex) const
{
    Frustum frustum = {};

    CameraMatrices* matrices = static_cast<CameraMatrices*>(mpUniforms->getHostMap()) + frameInFlightIndex;
    glm::mat4 vp = matrices->proj * matrices->view;

    glm::vec4 r0 = getRow(vp, 0);
    glm::vec4 r1 = getRow(vp, 1);
    glm::vec4 r2 = getRow(vp, 2);
    glm::vec4 r3 = getRow(vp, 3);

    frustum.planes[0] = r3 + r0; // Left
    frustum.planes[1] = r3 - r0; // Right
    frustum.planes[2] = r3 + r1; // Bottom
    frustum.planes[3] = r3 - r1; // Top
    frustum.planes[4] = r2;      // Near
    frustum.planes[5] = r3 - r2; // Far

    for (uint32_t i = 0; i < 6; i++) {
        frustum.planes[i] = normalizePlane(frustum.planes[i]);
    }

    return frustum;
}

void Camera::updateProjectionMatrix()
{
    switch (mProjectionType) {
    case CAMERA_PROJECTION_PERSPECTIVE:
        mProjection = glm::perspective(glm::radians(mFov), mAspectRatio, mNear, mFar);
        break;
    case CAMERA_PROJECTION_ORTHOGRAPHIC: {
        float orthoHeight = mOrthoSize;
        float orthoWidth = orthoHeight * mAspectRatio;
        mProjection =
            glm::ortho(-orthoWidth / 2.0f, orthoWidth / 2.0f, -orthoHeight / 2.0f, orthoHeight / 2.0f, mNear, mFar);
        break;
    }
    default:
        break;
    }
}