#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Descriptor.h"
#include "Device.h"
#include "Swapchain.h"

namespace Mandrill
{
    struct CameraMatrices {
        glm::mat4 view;
        glm::mat4 view_inv;
        glm::mat4 proj;
        glm::mat4 proj_inv;
    };

    /// <summary>
    /// Camera class for managing camera properties and transformations in a 3D scene.
    /// </summary>
    class Camera
    {
    public:
        MANDRILL_NON_COPYABLE(Camera)

        /// <summary>
        /// Create a new camera.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="pWindow">Window to use</param>
        /// <param name="pSwapchain">Swapchain to use</param>
        MANDRILL_API Camera(ptr<Device> pDevice, GLFWwindow* pWindow, ptr<Swapchain> pSwapchain);

        /// <summary>
        /// Destructor of camera.
        /// </summary>
        MANDRILL_API ~Camera();

        /// <summary>
        /// Create the descriptor for the camera, containing the view and projection matrices, and their invertations.
        /// </summary>
        /// <param name="stageFlags">Which shader stages the camera matrices will be used in</param>
        MANDRILL_API void createDescriptor(VkShaderStageFlags stageFlags);

        /// <summary>
        /// Create the ray tracing descriptor for the camera, containing the view and projection matrices, and their
        /// invertations.
        /// </summary>
        /// <param name="stageFlags">Which shader stages the camera matrices will be used in</param>
        MANDRILL_API void createRayTracingDescriptor(VkShaderStageFlags stageFlags);

        /// <summary>
        /// Update the aspect ratio that is used for the camera matrix. Call this if the window size changes.
        /// </summary>
        MANDRILL_API void updateAspectRatio();

        /// <summary>
        /// Update function to handle camera movements. Call this each app update.
        /// </summary>
        /// <param name="delta">Time since last update</param>
        /// <param name="cursorDelta">Mouse cursor movement</param>
        MANDRILL_API void update(float delta, glm::vec2 cursorDelta);

        /// <summary>
        /// Check if camera has captured the mouse movements.
        /// </summary>
        /// <returns>True if captured, otherwise false</returns>
        MANDRILL_API bool isMouseCaptured() const
        {
            return mMouseCaptured;
        }

        /// <summary>
        /// Set the capture state of the mouse.
        /// </summary>
        /// <param name="capture">State to set</param>
        MANDRILL_API void captureMouse(bool capture)
        {
            mMouseCaptured = capture;
        }

        /// <summary>
        /// Toggle mouse capture state, without specifying the new state.
        /// </summary>
        /// <returns>True if mouse is captured, otherwise false</returns>
        MANDRILL_API bool toggleMouseCapture()
        {
            mMouseCaptured = !mMouseCaptured;
            return mMouseCaptured;
        }

        /// <summary>
        /// Get the position of the camera.
        /// </summary>
        /// <returns>Position of the camera</returns>
        MANDRILL_API glm::vec3 getPosition() const
        {
            return mPosition;
        }

        /// <summary>
        /// Set the position of the camera.
        /// </summary>
        /// <param name="pos">New position</param>
        MANDRILL_API void setPosition(glm::vec3 pos)
        {
            mPosition = pos;
        }

        /// <summary>
        /// Get the direction of the camera.
        /// </summary>
        /// <returns>Direction of the camera</returns>
        MANDRILL_API glm::vec3 getDirection() const
        {
            return mDirection;
        }

        /// <summary>
        /// Set the direction of the camera.
        /// </summary>
        /// <param name="dir">New direction</param>
        MANDRILL_API void setDirection(glm::vec3 dir)
        {
            mDirection = dir;
        }

        /// <summary>
        /// Set the target the camera should look towards.
        /// </summary>
        /// <param name="target">New target</param>
        MANDRILL_API void setTarget(glm::vec3 target)
        {
            mDirection = glm::normalize(target - mPosition);
        }

        /// <summary>
        /// Set the up direction of the camera.
        /// </summary>
        /// <param name="up">New up direciton</param>
        MANDRILL_API void setUp(glm::vec3 up)
        {
            mUp = up;
        }

        /// <summary>
        /// Set the field of view of the camera.
        /// </summary>
        /// <param name="fov">New field of view</param>
        MANDRILL_API void setFov(float fov)
        {
            mFov = fov;
        }

        /// <summary>
        /// Set near and far planes.
        /// </summary>
        /// <param name="nearPlane">Distance to near plane</param>
        /// <param name="farPlane">Distance to far plane</param>
        MANDRILL_API void setNearFar(float nearPlane, float farPlane)
        {
            mNear = nearPlane;
            mFar = farPlane;
        }

        /// <summary>
        /// Set the movement speed of the camera.
        /// </summary>
        /// <param name="speed">New speed</param>
        MANDRILL_API void setMoveSpeed(float speed)
        {
            mMoveSpeed = speed;
        }

        /// <summary>
        /// Get the view matrix of the camera.
        /// </summary>
        /// <returns>View matrix</returns>
        MANDRILL_API glm::mat4 getViewMatrix() const
        {
            CameraMatrices* matrices =
                static_cast<CameraMatrices*>(mpUniforms->getHostMap()) + mpSwapchain->getInFlightIndex();
            return matrices->view;
        }

        /// <summary>
        /// Get the projection matrix of the camera.
        /// </summary>
        /// <returns>Projection matrix</returns>
        MANDRILL_API glm::mat4 getProjectionMatrix() const
        {
            CameraMatrices* matrices =
                static_cast<CameraMatrices*>(mpUniforms->getHostMap()) + mpSwapchain->getInFlightIndex();
            return matrices->proj;
        }

        /// <summary>
        /// Get the descriptor of the camera, containing the view and projection matrices, and their invertations.
        /// </summary>
        /// <returns>Descriptor</returns>
        MANDRILL_API ptr<Descriptor> getDescriptor() const
        {
            return mpDescriptor;
        }

        /// <summary>
        /// Get the ray tracing descriptor of the camera, containing the view and projection matrices, and their
        /// invertations.
        /// </summary>
        /// <returns>Descriptor</returns>
        MANDRILL_API ptr<Descriptor> getRayTracingDescriptor() const
        {
            return mpRayTracingDescriptor;
        }

    private:
        ptr<Device> mpDevice;
        GLFWwindow* mpWindow;
        ptr<Swapchain> mpSwapchain;

        bool mMouseCaptured = false;

        float mAspect;
        float mNear, mFar;
        float mFov;
        glm::vec3 mPosition;
        glm::vec3 mDirection;
        glm::vec3 mUp;
        float mMoveSpeed;

        ptr<Buffer> mpUniforms;
        ptr<Descriptor> mpDescriptor;
        ptr<Descriptor> mpRayTracingDescriptor;
        VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout mRayTracingDescriptorSetLayout = VK_NULL_HANDLE;
    };
} // namespace Mandrill
