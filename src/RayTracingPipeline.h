#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Descriptor.h"
#include "Device.h"
#include "Layout.h"
#include "Pipeline.h"
#include "Shader.h"


namespace Mandrill
{
    struct RayTracingPipelineDesc {
        uint32_t maxRecursionDepth;
        uint32_t missGroupCount;
        uint32_t hitGroupCount;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;

        MANDRILL_API RayTracingPipelineDesc(uint32_t missGroupCount = 1, uint32_t hitGroupCount = 1,
                                            uint32_t maxRecursionDepth = 1)
            : maxRecursionDepth(maxRecursionDepth), missGroupCount(missGroupCount), hitGroupCount(hitGroupCount)
        {
            shaderGroups.resize(1 + missGroupCount + hitGroupCount);
        }

        MANDRILL_API void setRayGen(uint32_t stage)
        {
            VkRayTracingShaderGroupCreateInfoKHR ci = {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = stage,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
            };
            shaderGroups[0] = ci;
        }

        MANDRILL_API void setMissGroup(uint32_t missGroup, uint32_t stage)
        {
            if (missGroup >= missGroupCount) {
                Log::error("Miss group {} exceeds hitGroupCount {}", missGroup, missGroupCount);
                return;
            }

            VkRayTracingShaderGroupCreateInfoKHR ci = {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = stage,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
            };
            shaderGroups[1 + missGroup] = ci;
        }

        MANDRILL_API void setHitGroup(uint32_t hitGroup, uint32_t stage)
        {
            if (hitGroup >= hitGroupCount) {
                Log::error("Hit group {} exceeds hitGroupCount {}", hitGroup, hitGroupCount);
                return;
            }

            VkRayTracingShaderGroupCreateInfoKHR ci = {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = VK_SHADER_UNUSED_KHR,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
            };
            shaderGroups[1 + missGroupCount + hitGroup] = ci;
        }
    };

    class RayTracingPipeline : public Pipeline
    {
    public:
        MANDRILL_API RayTracingPipeline(ptr<Device> pDevice, ptr<Shader> pShader, ptr<Layout> pLayout,
                                        const RayTracingPipelineDesc& desc = RayTracingPipelineDesc());
        MANDRILL_API ~RayTracingPipeline();

        MANDRILL_API void bind(VkCommandBuffer cmd);

        MANDRILL_API void write(VkCommandBuffer cmd, VkImage image);

        MANDRILL_API void present(VkCommandBuffer cmd, VkImage image);

        MANDRILL_API void recreate();

        MANDRILL_API VkStridedDeviceAddressRegionKHR getRayGenSBT() const
        {
            VkDeviceAddress address = mpShaderBindingTableBuffer->getDeviceAddress();
            VkStridedDeviceAddressRegionKHR region = {
                .deviceAddress = address,
                .stride = mGroupSizeAligned,
                .size = mGroupSizeAligned,
            };
            return region;
        }

        MANDRILL_API VkStridedDeviceAddressRegionKHR getMissSBT() const
        {
            VkDeviceAddress address = mpShaderBindingTableBuffer->getDeviceAddress();
            VkStridedDeviceAddressRegionKHR region = {
                .deviceAddress = address + mGroupSizeAligned,
                .stride = mGroupSizeAligned,
                .size = mGroupSizeAligned,
            };
            return region;
        }

        MANDRILL_API VkStridedDeviceAddressRegionKHR getHitSBT() const
        {
            VkDeviceAddress address = mpShaderBindingTableBuffer->getDeviceAddress();
            VkStridedDeviceAddressRegionKHR region = {
                .deviceAddress = address + mGroupSizeAligned * (1 + mMissGroupCount),
                .stride = mGroupSizeAligned,
                .size = mGroupSizeAligned,
            };
            return region;
        }

        MANDRILL_API VkStridedDeviceAddressRegionKHR getCallSBT() const
        {
            // Not implemented
            VkStridedDeviceAddressRegionKHR region = {0};
            return region;
        }

    private:
        void createPipeline();
        void destroyPipeline();

        void createShaderBindingTable();
        void createDescriptor();

        ptr<Device> mpDevice;

        ptr<Shader> mpShader;
        ptr<Layout> mpLayout;

        VkPipeline mPipeline;
        VkPipelineLayout mPipelineLayout;

        std::vector<VkRayTracingShaderGroupCreateInfoKHR> mShaderGroups;

        ptr<Buffer> mpShaderBindingTableBuffer;
        uint32_t mGroupSizeAligned;

        uint32_t mMaxRecursionDepth;
        uint32_t mMissGroupCount;
        uint32_t mHitGroupCount;

        ptr<Descriptor> mpStorageImageDescriptor;
    };
} // namespace Mandrill
