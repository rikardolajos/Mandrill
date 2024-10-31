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

    class Camera
    {
    public:
        MANDRILL_API Camera(ptr<Device> pDevice, GLFWwindow* pWindow, ptr<Swapchain> pSwapchain);
        MANDRILL_API ~Camera();

        /// <summary>
        /// Update the aspect ratio that is used for the camera matrix. Call this if the window size changes.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void updateAspectRatio();

        MANDRILL_API void update(float delta, glm::vec2 cursorDelta);

        MANDRILL_API VkWriteDescriptorSet getWriteDescriptor(uint32_t binding) const;

        MANDRILL_API bool isMouseCaptured() const
        {
            return mMouseCaptured;
        }

        MANDRILL_API void captureMouse(bool capture)
        {
            mMouseCaptured = capture;
        }

        MANDRILL_API bool toggleMouseCapture()
        {
            mMouseCaptured = !mMouseCaptured;
            return mMouseCaptured;
        }

        MANDRILL_API glm::vec3 getPosition() const
        {
            return mPosition;
        }

        MANDRILL_API void setPosition(glm::vec3 pos)
        {
            mPosition = pos;
        }

        MANDRILL_API glm::vec3 getDirection() const
        {
            return mDirection;
        }

        MANDRILL_API void setDirection(glm::vec3 dir)
        {
            mDirection = dir;
        }

        MANDRILL_API void setTarget(glm::vec3 target)
        {
            mDirection = glm::normalize(target - mPosition);
        }

        MANDRILL_API void setUp(glm::vec3 up)
        {
            mUp = up;
        }

        MANDRILL_API void setFov(float fov)
        {
            mFov = fov;
        }

        MANDRILL_API void setMoveSpeed(float speed)
        {
            mMoveSpeed = speed;
        }

        MANDRILL_API glm::mat4 getViewMatrix() const
        {
            CameraMatrices* matrices =
                static_cast<CameraMatrices*>(mpUniforms->getHostMap()) + mpSwapchain->getInFlightIndex();
            return matrices->view;
        }

        MANDRILL_API glm::mat4 getProjectionMatrix() const
        {
            CameraMatrices* matrices =
                static_cast<CameraMatrices*>(mpUniforms->getHostMap()) + mpSwapchain->getInFlightIndex();
            return matrices->proj;
        }

        MANDRILL_API VkDescriptorSet getDescriptorSet() const
        {
            return mpDescriptor->getSet(mpSwapchain->getInFlightIndex());
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
        VkDescriptorSetLayout mLayout;
        VkDescriptorBufferInfo mBufferInfo;
    };
} // namespace Mandrill