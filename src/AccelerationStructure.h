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

        ptr<Buffer> pBuffer;
    };

    // Forward declare Descriptor and Scene
    class Descriptor;
    class Scene;

    class AccelerationStructure : public std::enable_shared_from_this<AccelerationStructure>
    {
    public:
        MANDRILL_API AccelerationStructure(ptr<Device> pDevice, ptr<Scene> pScene,
                                           VkBuildAccelerationStructureFlagsKHR flags);
        MANDRILL_API ~AccelerationStructure();

        /// <summary>
        /// Rebuild the acceleration structure from scratch.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void rebuild(VkBuildAccelerationStructureFlagsKHR flags);

        /// <summary>
        /// Refit the acceleration structure to the new positions of the nodes.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void refit();

        /// <summary>
        /// Get the TLAS acceleration structure handle.
        /// </summary>
        /// <returns>A VkAccelerationStructureKHR handle</returns>
        MANDRILL_API VkAccelerationStructureKHR getAccelerationStructure() const
        {
            return mTLAS;
        }

    private:
        /// <summary>
        /// Create the bottom levels of the acceleration structure. One BLAS per node in scene.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void createBLASes(VkBuildAccelerationStructureFlagsKHR flags);

        /// <summary>
        /// Create the top level of the acceleration structure.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void createTLAS(VkBuildAccelerationStructureFlagsKHR flags);

        ptr<Device> mpDevice;
        ptr<Scene> mpScene;

        VkAccelerationStructureKHR mTLAS;
        VkAccelerationStructureGeometryKHR mGeometry;
        VkAccelerationStructureBuildRangeInfoKHR mBuildRange;

        AccelerationStructureBuildInfo mBuildInfo;

        std::vector<BLAS> mBLASes;

        ptr<Buffer> mpBuffer;
        ptr<Buffer> mpScratch;
        ptr<Buffer> mpInstances;

        ptr<Descriptor> mpDescriptor;
    };
} // namespace Mandrill