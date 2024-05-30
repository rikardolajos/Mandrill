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

        MANDRILL_API void update(float delta);

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

        MANDRILL_API VkWriteDescriptorSet getDescriptor(uint32_t binding) const;

    private:
        std::shared_ptr<Device> mpDevice;
        GLFWwindow* mpWindow;

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