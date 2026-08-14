#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"
#include "DynamicBuffer.h"
#include "Frustum.h"
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
        /// Create a new camera. The matrices are kept in one copy per frame in flight, as the device is set up for.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        MANDRILL_API Camera(ptr<Device> pDevice);

        /// <summary>
        /// Destructor of camera.
        /// </summary>
        MANDRILL_API ~Camera();

        /// <summary>
        /// Update function to update uniforms, without any user input movement.
        /// </summary>
        /// <param name="frameInFlightIndex">Which copy of the matrices to write, the current frame by default</param>
        MANDRILL_API void update(uint32_t frameInFlightIndex = kCurrentFrameInFlight);

        /// <summary>
        /// Update function to handle camera movements. Call this each app update.
        /// </summary>
        /// <param name="pWindow">GLFW window to poll for input</param>
        /// <param name="delta">Time since last update</param>
        /// <param name="cursorDelta">Mouse cursor movement</param>
        /// <param name="frameInFlightIndex">Which copy of the matrices to write, the current frame by default</param>
        MANDRILL_API void update(GLFWwindow* pWindow, float delta, glm::vec2 cursorDelta,
                                 uint32_t frameInFlightIndex = kCurrentFrameInFlight);

        /// <summary>
        /// Set the projection of the camera.
        /// </summary>
        /// <param name="projectionType">Perspective or othographic projection</param>
        MANDRILL_API void setProjection(const CameraProjection projectionType);

        /// <summary>
        /// Get the frustum of the camera.
        /// </summary>
        /// <param name="frameInFlightIndex">Which copy of the matrices to read, the current frame by default</param>
        /// <returns>Camera frustum</returns>
        MANDRILL_API Frustum getFrustum(uint32_t frameInFlightIndex = kCurrentFrameInFlight) const;

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
        /// <param name="frameInFlightIndex">Which copy of the matrices to read, the current frame by default</param>
        /// <returns>View matrix</returns>
        MANDRILL_API glm::mat4 getViewMatrix(uint32_t frameInFlightIndex = kCurrentFrameInFlight) const
        {
            CameraMatrices* matrices = static_cast<CameraMatrices*>(mpUniforms->at(frameInFlightIndex));
            return matrices->view;
        }

        /// <summary>
        /// Get the projection matrix of the camera.
        /// </summary>
        /// <param name="frameInFlightIndex">Which copy of the matrices to read, the current frame by default</param>
        /// <returns>Projection matrix</returns>
        MANDRILL_API glm::mat4 getProjectionMatrix(uint32_t frameInFlightIndex = kCurrentFrameInFlight) const
        {
            CameraMatrices* matrices = static_cast<CameraMatrices*>(mpUniforms->at(frameInFlightIndex));
            return matrices->proj;
        }

        /// <summary>
        /// Get the buffer containing the camera matrices, with one copy per frame in flight. Attach this to a shader
        /// resource, or use it for custom descriptor creation.
        /// </summary>
        /// <returns>Uniform buffer</returns>
        MANDRILL_API ptr<DynamicBuffer> getUniformBuffer() const
        {
            return mpUniforms;
        }

        /// <summary>
        /// Get the offset that selects a frame's copy of the camera matrices when binding a descriptor. Only needed
        /// when binding a descriptor by hand, a shader that the uniform buffer is attached to works this out itself.
        /// </summary>
        /// <param name="frameInFlightIndex">Which copy of the matrices to select, the current frame by default</param>
        /// <returns>Dynamic offset in bytes</returns>
        MANDRILL_API uint32_t getDynamicOffset(uint32_t frameInFlightIndex = kCurrentFrameInFlight) const
        {
            return mpUniforms->getOffset(frameInFlightIndex);
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

        ptr<DynamicBuffer> mpUniforms;
    };
} // namespace Mandrill
