#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"
#include "Scene.h"

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


    class AccelerationStructure
    {
    public:
        MANDRILL_API AccelerationStructure(ptr<Device> pDevice, ptr<Scene> pScene);
        MANDRILL_API ~AccelerationStructure();

        /// <summary>
        /// Rebuild the acceleration structure from scratch.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void rebuild();

        /// <summary>
        /// Refit the acceleration structure to the new positions of the nodes.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void refit();

    private:
        /// <summary>
        /// Create the bottom levels of the acceleration structure. One BLAS per node in scene.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void createBLASes();

        /// <summary>
        /// Create the top level of the acceleration structure.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void createTLAS();

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