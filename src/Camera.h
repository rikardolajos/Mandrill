#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Descriptor.h"
#include "Device.h"
#include "Swapchain.h"

namespace Mandrill
{
    enum CameraProjection {
        CAMERA_PROJECTION_PERSPECTIVE,
        CAMERA_PROJECTION_ORTHOGRAPHIC,
        CAMERA_PROJECTION_COUNT,
    };

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
        /// <param name="framesInFlightCount">Used to determine how many copies of per-frame resources are
        /// needed</param>
        MANDRILL_API Camera(ptr<Device> pDevice, uint32_t framesInFlightCount);

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
        /// Update function to update uniforms, without any user input movement.
        /// </summary>
        /// <param name="frameInFlightIndex">Used to determine which resource to use</param>
        MANDRILL_API void update(uint32_t frameInFlightIndex);

        /// <summary>
        /// Update function to handle camera movements. Call this each app update.
        /// </summary>
        /// <param name="pWindow">GLFW window to poll for input</param>
        /// <param name="delta">Time since last update</param>
        /// <param name="cursorDelta">Mouse cursor movement</param>
        /// <param name="frameInFlightIndex">Used to determine which resource to use</param>
        MANDRILL_API void update(GLFWwindow* pWindow, float delta, glm::vec2 cursorDelta, uint32_t frameInFlightIndex);

        /// <summary>
        /// Set the projection of the camera.
        /// </summary>
        /// <param name="projectionType">Perspective or othographic projection</param>
        MANDRILL_API void setProjection(const CameraProjection projectionType);

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
        /// Set the aspect ratio of the camera. Width / height.
        /// </summary>
        /// <param name="aspect">Aspect ratio</param>
        MANDRILL_API void setAspectRatio(float aspectRatio)
        {
            mAspectRatio = aspectRatio;
            updateProjectionMatrix();
        }

        /// <summary>
        /// Set the orthographic size of the camera.
        /// </summary>
        /// <param name="orthoSize">Orthographics size</param>
        MANDRILL_API void setOrthoSize(float orthoSize)
        {
            mOrthoSize = orthoSize;
            updateProjectionMatrix();
        }

        /// <summary>
        /// Set the field of view of the camera.
        /// </summary>
        /// <param name="fov">New field of view</param>
        MANDRILL_API void setFov(float fov)
        {
            mFov = fov;
            updateProjectionMatrix();
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
            updateProjectionMatrix();
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
        /// <param name="frameInFlightIndex">Used to determine which resource to use</param>
        /// <returns>View matrix</returns>
        MANDRILL_API glm::mat4 getViewMatrix(uint32_t frameInFlightIndex) const
        {
            CameraMatrices* matrices = static_cast<CameraMatrices*>(mpUniforms->getHostMap()) + frameInFlightIndex;
            return matrices->view;
        }

        /// <summary>
        /// Get the projection matrix of the camera.
        /// </summary>
        /// <param name="frameInFlightIndex">Used to determine which resource to use</param>
        /// <returns>Projection matrix</returns>
        MANDRILL_API glm::mat4 getProjectionMatrix(uint32_t frameInFlightIndex) const
        {
            CameraMatrices* matrices = static_cast<CameraMatrices*>(mpUniforms->getHostMap()) + frameInFlightIndex;
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

        /// <summary>
        /// Get the buffer containing the camera matrices. This can be used for custom descriptor creation.
        /// </summary>
        /// <returns>Uniform buffer</returns>
        MANDRILL_API ptr<Buffer> getUniformBuffer() const
        {
            return mpUniforms;
        }

    private:
        void updateProjectionMatrix();

        ptr<Device> mpDevice;

        bool mMouseCaptured = false;

        float mAspectRatio;
        float mNear, mFar;
        float mFov;
        float mOrthoSize;
        CameraProjection mProjectionType = CAMERA_PROJECTION_PERSPECTIVE;
        glm::mat4 mProjection;
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
