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
                Log::Error("Miss group {} exceeds hitGroupCount {}", missGroup, missGroupCount);
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

        MANDRILL_API void
        setHitGroup(uint32_t hitGroup, uint32_t closestHitStage, uint32_t anyHitStage = VK_SHADER_UNUSED_KHR,
                    uint32_t intersectionStage = VK_SHADER_UNUSED_KHR,
                    VkRayTracingShaderGroupTypeKHR type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR)
        {
            if (hitGroup >= hitGroupCount) {
                Log::Error("Hit group {} exceeds hitGroupCount {}", hitGroup, hitGroupCount);
                return;
            }

            VkRayTracingShaderGroupCreateInfoKHR ci = {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                .generalShader = VK_SHADER_UNUSED_KHR,
                .closestHitShader = closestHitStage,
                .anyHitShader = anyHitStage,
                .intersectionShader = intersectionStage,
            };
            shaderGroups[1 + missGroupCount + hitGroup] = ci;
        }
    };

    /// <summary>
    /// Ray tracing pipeline class that manages the creation and usage of a ray tracing pipeline in Vulkan.
    /// </summary>
    class RayTracingPipeline : public Pipeline
    {
    public:
        MANDRILL_NON_COPYABLE(RayTracingPipeline)

        /// <summary>
        /// Create a new ray tracing pipeline.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="pShader">Shader to use</param>
        /// <param name="desc">Description of pipeline</param>
        MANDRILL_API RayTracingPipeline(ptr<Device> pDevice, ptr<Shader> pShader,
                                        const RayTracingPipelineDesc& desc = RayTracingPipelineDesc());

        /// <summary>
        /// Bind pipeline for rendering.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <returns></returns>
        MANDRILL_API void bind(VkCommandBuffer cmd);

        /// <summary>
        /// Transition an image for writing to.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="image">Image to transition</param>
        /// <returns></returns>
        MANDRILL_API void write(VkCommandBuffer cmd, VkImage image);

        /// <summary>
        /// Transition and image for reading from.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="image">Image to transition</param>
        /// <returns></returns>
        MANDRILL_API void read(VkCommandBuffer cmd, VkImage image);

        /// <summary>
        /// Recreate a pipeline. Call this if shader source code has changed and should be reloaded.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void recreate();

        /// <summary>
        /// Get the raygen group shader binding table record.
        /// </summary>
        /// <returns>Device address for raygen group</returns>
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

        /// <summary>
        /// Get the miss group shader binding table record.
        /// </summary>
        /// <returns>Device address for miss group</returns>
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

        /// <summary>
        /// Get the hit group shader binding table record.
        /// </summary>
        /// <returns>Device address for hit group</returns>
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

        /// <summary>
        /// [NOT IMPLEMENTED] Get the call group shader binding table record.
        /// </summary>
        /// <returns>Device address for call group</returns>
        MANDRILL_API VkStridedDeviceAddressRegionKHR getCallSBT() const
        {
            // Not implemented
            VkStridedDeviceAddressRegionKHR region = {0};
            return region;
        }

    private:
        void createPipeline();
        void createShaderBindingTable();

        std::vector<VkRayTracingShaderGroupCreateInfoKHR> mShaderGroups;

        ptr<Buffer> mpShaderBindingTableBuffer;
        uint32_t mGroupSizeAligned;

        uint32_t mMaxRecursionDepth;
        uint32_t mMissGroupCount;
        uint32_t mHitGroupCount;

        ptr<Descriptor> mpStorageImageDescriptor;
    };
} // namespace Mandrill
