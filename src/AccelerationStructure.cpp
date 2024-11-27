#include "AccelerationStructure.h"

#include "Descriptor.h"
#include "Error.h"
#include "Extension.h"
#include "Helpers.h"
#include "Scene.h"

using namespace Mandrill;


AccelerationStructure::AccelerationStructure(ptr<Device> pDevice, ptr<Scene> pScene,
                                             VkBuildAccelerationStructureFlagsKHR flags)
    : mpDevice(pDevice), mpScene(pScene)
{
    if (mpScene->getNodes().empty()) {
        Log::error("Cannot build acceleration structure of empty scene");
        return;
    }

    mBLASes.resize(mpScene->getNodes().size());

    createBLASes(flags);
    createTLAS(flags);
}

AccelerationStructure::~AccelerationStructure()
{
    for (auto& blas : mBLASes) {
        vkDestroyAccelerationStructureKHR(mpDevice->getDevice(), blas.accelerationStructure, nullptr);
    }

    vkDestroyAccelerationStructureKHR(mpDevice->getDevice(), mTLAS, nullptr);
}

void AccelerationStructure::rebuild(VkBuildAccelerationStructureFlagsKHR flags)
{
}

void AccelerationStructure::refit()
{
}

void AccelerationStructure::createBLASes(VkBuildAccelerationStructureFlagsKHR flags)
{
    VkDeviceSize scratchSize = 0;

    // Loop over the meshes in the scene
    uint32_t i = 0;
    for (auto& node : mpScene->getNodes()) {
        for (auto& meshIndex : node.getMeshIndices()) {
            BLAS* blas = &mBLASes[i];

            VkDeviceOrHostAddressConstKHR vertexAddress = {.deviceAddress = mpScene->getMeshVertexAddress(meshIndex)};
            VkDeviceOrHostAddressConstKHR indexAddress = {.deviceAddress = mpScene->getMeshIndexAddress(meshIndex)};

            VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData = vertexAddress,
                .vertexStride = sizeof(Vertex),
                .maxVertex = mpScene->getMeshVertexCount(meshIndex) - 1,
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData = indexAddress,
                .transformData = {0}, // Identity transform
            };

            VkAccelerationStructureGeometryDataKHR geometry = {
                .triangles = triangles,
            };

            blas->geometry = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                .geometry = geometry,
                .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
            };

            blas->buildRange = {
                .primitiveCount = mpScene->getMeshIndexCount(meshIndex),
                .primitiveOffset = 0,
                .firstVertex = 0,
                .transformOffset = 0,
            };

            blas->buildInfo = {
                .geometry =
                    {
                        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                        .flags = flags,
                        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
                        .srcAccelerationStructure = VK_NULL_HANDLE,
                        .geometryCount = 1,
                        .pGeometries = &blas->geometry,
                    },
            };

            blas->buildInfo.size = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
            };

            vkGetAccelerationStructureBuildSizesKHR(
                mpDevice->getDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &blas->buildInfo.geometry,
                &blas->buildRange.primitiveCount, &blas->buildInfo.size);

            scratchSize = std::max(scratchSize, blas->buildInfo.size.accelerationStructureSize);

            blas->buildInfo.range = &blas->buildRange;

            // Allocate buffer for the BLAS
            blas->pBuffer =
                make_ptr<Buffer>(mpDevice, blas->buildInfo.size.accelerationStructureSize,
                                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            VkAccelerationStructureCreateInfoKHR ci = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                .buffer = blas->pBuffer->getBuffer(),
                .offset = 0,
                .size = blas->buildInfo.size.accelerationStructureSize,
                .type = blas->buildInfo.geometry.type,
            };

            Check::Vk(
                vkCreateAccelerationStructureKHR(mpDevice->getDevice(), &ci, nullptr, &blas->accelerationStructure));

            i += 1;
        }
    }

    // Allocate scratch buffer
    mpScratch = make_ptr<Buffer>(mpDevice, scratchSize,
                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceAddress scratchAddress = mpScratch->getDeviceAddress();

    // Build in batches of 256 MB
    uint32_t batchStart = 0;
    uint32_t batchLength = 0;
    VkDeviceSize batchSize = 0;
    const VkDeviceSize batchMemoryLimit = 256'000'000;

    for (uint32_t i = 0; i < static_cast<uint32_t>(mBLASes.size()); i++) {
        BLAS* blas = &mBLASes[i];

        batchLength += 1;
        batchSize += blas->buildInfo.size.accelerationStructureSize;

        blas->buildInfo.geometry.dstAccelerationStructure = blas->accelerationStructure;
        blas->buildInfo.geometry.scratchData.deviceAddress = scratchAddress;

        if (batchSize >= batchMemoryLimit || i == mBLASes.size() - 1) {
            VkCommandBuffer cmd = Helpers::cmdBegin(mpDevice);
            for (uint32_t j = batchStart; j < batchLength; j++) {
                BLAS* b = &mBLASes[j];
                vkCmdBuildAccelerationStructuresKHR(cmd, 1, &b->buildInfo.geometry, &b->buildInfo.range);

                // Synchronize access to the scratch area
                VkMemoryBarrier barrier = {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                    .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                };
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                     VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr,
                                     0, nullptr);
            }

            Helpers::cmdEnd(mpDevice, cmd);

            batchLength = 0;
            batchSize = 0;
            batchStart = i + 1;
        }
    }
}

void AccelerationStructure::createTLAS(VkBuildAccelerationStructureFlagsKHR flags)
{
    auto instances = std::vector<VkAccelerationStructureInstanceKHR>(mBLASes.size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(mBLASes.size()); i++) {
        VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = mBLASes[i].accelerationStructure,
        };

        VkDeviceAddress address = vkGetAccelerationStructureDeviceAddressKHR(mpDevice->getDevice(), &addressInfo);

        VkTransformMatrixKHR transform = {0};
        transform.matrix[0][0] = 1.0f;
        transform.matrix[1][1] = 1.0f;
        transform.matrix[2][2] = 1.0f;
        instances[i] = {
            .transform = transform,
            .instanceCustomIndex = i,
            .mask = 0xff,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = address,
        };
    }

    VkDeviceSize size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    mpInstances = make_ptr<Buffer>(mpDevice, size,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mpInstances->copyFromHost(instances.data(), size, 0);

    VkDeviceOrHostAddressConstKHR deviceAddress = {.deviceAddress = mpInstances->getDeviceAddress()};
    VkAccelerationStructureGeometryInstancesDataKHR instanceData = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data = deviceAddress,
    };

    VkAccelerationStructureGeometryDataKHR geometry = {.instances = instanceData};
    mGeometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = geometry,
    };

    mBuildInfo.geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = flags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &mGeometry,
    };

    mBuildInfo.size = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
    };

    vkGetAccelerationStructureBuildSizesKHR(mpDevice->getDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &mBuildInfo.geometry, &mBuildRange.primitiveCount, &mBuildInfo.size);

    // Allocate buffer for the TLAS
    mpBuffer = make_ptr<Buffer>(mpDevice, mBuildInfo.size.accelerationStructureSize,
                                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkAccelerationStructureCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = mpBuffer->getBuffer(),
        .offset = 0,
        .size = mBuildInfo.size.accelerationStructureSize,
        .type = mBuildInfo.geometry.type,
    };

    Check::Vk(vkCreateAccelerationStructureKHR(mpDevice->getDevice(), &ci, nullptr, &mTLAS));

    // Create bigger scratch buffer if needed
    VkDeviceSize scratchSize = std::max(mBuildInfo.size.accelerationStructureSize, mBuildInfo.size.updateScratchSize);
    if (scratchSize > mpScratch->getSize()) {
        VkBufferUsageFlags usage = mpScratch->getUsage();
        VkMemoryPropertyFlags properties = mpScratch->getProperties();
        mpScratch = make_ptr<Buffer>(mpDevice, scratchSize, usage, properties);
    }

    mBuildInfo.geometry.dstAccelerationStructure = mTLAS;
    mBuildInfo.geometry.scratchData.deviceAddress = mpScratch->getDeviceAddress();

    mBuildRange = {
        .primitiveCount = static_cast<uint32_t>(mBLASes.size()),
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };

    mBuildInfo.range = &mBuildRange;

    VkCommandBuffer cmd = Helpers::cmdBegin(mpDevice);
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &mBuildInfo.geometry, &mBuildInfo.range);
    Helpers::cmdEnd(mpDevice, cmd);
}
