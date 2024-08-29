#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"

namespace Mandrill
{
    class Camera
    {
    public:
        MANDRILL_API Camera(std::shared_ptr<Device> pDevice, GLFWwindow* pWindow);
        MANDRILL_API ~Camera();

        /// <summary>
        /// Update the aspect ratio that is used for the camera matrix. Call this if the window size changes.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void updateAspectRatio();

        MANDRILL_API void update(float delta, glm::vec2 cursorDelta);

        MANDRILL_API bool isMouseCaptured()
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

        MANDRILL_API void setPosition(glm::vec3 pos)
        {
            mPosition = pos;
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

        MANDRILL_API VkWriteDescriptorSet getDescriptor(uint32_t binding) const;

    private:
        std::shared_ptr<Device> mpDevice;
        GLFWwindow* mpWindow;

        bool mMouseCaptured = false;

        float mAspect;
        float mNear, mFar;
        float mFov;
        glm::vec3 mPosition;
        glm::vec3 mDirection;
        glm::vec3 mUp;
        float mMoveSpeed;
        std::shared_ptr<Buffer> mpUniforms;
        VkDescriptorBufferInfo mBufferInfo;
    };
} // namespace Mandrill