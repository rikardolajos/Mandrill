#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"

namespace Mandrill
{
    struct AccelerationStructureBuildInfo {
        VkAccelerationStructureBuildGeometryInfoKHR geometry;
        VkAccelerationStructureBuildSizesInfoKHR size;
        const VkAccelerationStructureBuildRangeInfoKHR* range;
    };

    struct BLAS {
        VkAccelerationStructureKHR accelerationStructure;
        VkAccelerationStructureGeometryKHR geometry;
        VkAccelerationStructureBuildRangeInfoKHR buildRange;

        AccelerationStructureBuildInfo buildInfo;
    };

    // Forward declare Descriptor and Scene
    class Descriptor;
    class Scene;

    class AccelerationStructure : public std::enable_shared_from_this<AccelerationStructure>
    {
    public:
        MANDRILL_NON_COPYABLE(AccelerationStructure)

        MANDRILL_API AccelerationStructure(ptr<Device> pDevice, ptr<Scene> pScene,
                                           VkBuildAccelerationStructureFlagsKHR flags);
        MANDRILL_API ~AccelerationStructure();

        /// <summary>
        /// Rebuild the top level of the acceleration structure to account for updates in instance transforms.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void update(VkBuildAccelerationStructureFlagsKHR flags);

        /// <summary>
        /// Get the TLAS acceleration structure handle.
        /// </summary>
        /// <returns>A VkAccelerationStructureKHR handle</returns>
        MANDRILL_API VkAccelerationStructureKHR getAccelerationStructure() const
        {
            return mTLAS;
        }

        /// <summary>
        /// Get the write descriptor of the acceleration structure.
        /// </summary>
        /// <param name="binding">Binding to assign to the write descriptor</param>
        /// <returns>A write descriptor</returns>
        MANDRILL_API VkWriteDescriptorSet getWriteDescriptor(uint32_t binding) const
        {
            static VkWriteDescriptorSetAccelerationStructureKHR as = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                .accelerationStructureCount = 1,
                .pAccelerationStructures = &mTLAS,
            };

            VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = &as,
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            };

            return write;
        }

    private:
        /// <summary>
        /// Create the bottom levels of the acceleration structure. One BLAS per node in scene.
        /// </summary>
        /// <param name="flags">Flags to set the build mode</param>
        /// <returns></returns>
        MANDRILL_API void createBLASes(VkBuildAccelerationStructureFlagsKHR flags);

        /// <summary>
        /// Create the top level of the acceleration structure.
        /// </summary>
        /// <param name="flags">Flags to set the build mode</param>
        /// <param name="update">Update to rebuild TLAS with updated matrices</param>
        /// <returns></returns>
        MANDRILL_API void createTLAS(VkBuildAccelerationStructureFlagsKHR flags, bool update = false);

        ptr<Device> mpDevice;
        ptr<Scene> mpScene;

        VkAccelerationStructureKHR mTLAS;
        VkAccelerationStructureGeometryKHR mGeometry;
        VkAccelerationStructureBuildRangeInfoKHR mBuildRange;

        AccelerationStructureBuildInfo mBuildInfo;

        std::vector<BLAS> mBLASes;

        ptr<Buffer> mpBLASBuffer;
        ptr<Buffer> mpTLASBuffer;
        ptr<Buffer> mpScratch;
        ptr<Buffer> mpInstances;
    };
} // namespace Mandrill