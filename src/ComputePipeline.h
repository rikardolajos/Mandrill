#pragma once

#include "Common.h"

#include "Device.h"
#include "Layout.h"
#include "Pipeline.h"
#include "Shader.h"

namespace Mandrill
{
    /// <summary>
    /// Configuration structure for creating a compute pipeline. A compute pipeline has no fixed-function state to
    /// configure, so this only carries the creation flags.
    /// </summary>
    struct ComputePipelineDesc {

        /// <summary>
        /// Pipeline creation flags. Defaults to none.
        /// </summary>
        VkPipelineCreateFlags flags = 0;

        /// <summary>
        /// Constructor for compute pipeline description.
        /// </summary>
        /// <param name="flags">Pipeline creation flags</param>
        MANDRILL_API ComputePipelineDesc(VkPipelineCreateFlags flags = 0) : flags(flags)
        {
        }
    };

    /// <summary>
    /// Compute pipeline class that manages the creation and usage of a Vulkan compute pipeline.
    ///
    /// The shader is expected to hold a single compute stage. The local workgroup size is taken from the shader
    /// reflection, so dispatch() can be given the number of work items and work out the group counts on its own.
    /// </summary>
    class ComputePipeline : public Pipeline
    {
    public:
        MANDRILL_NON_COPYABLE(ComputePipeline)

        /// <summary>
        /// Create a new compute pipeline.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="pShader">Shader to use, holding a compute stage</param>
        /// <param name="desc">Description of pipeline</param>
        MANDRILL_API ComputePipeline(ptr<Device> pDevice, ptr<Shader> pShader,
                                     const ComputePipelineDesc& desc = ComputePipelineDesc());

        /// <summary>
        /// Bind pipeline for dispatching.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        MANDRILL_API void bind(VkCommandBuffer cmd) override;

        /// <summary>
        /// Recreate a pipeline. Call this if shader source code has changed and should be reloaded.
        /// </summary>
        MANDRILL_API void recreate() override;

        /// <summary>
        /// Dispatch a number of work items, rounded up to whole workgroups using the local size of the shader. Use
        /// this when the shader guards against running past the end of its work, which it has to do whenever the work
        /// item count is not a multiple of the local size.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="workItemsX">Number of work items in x</param>
        /// <param name="workItemsY">Number of work items in y</param>
        /// <param name="workItemsZ">Number of work items in z</param>
        MANDRILL_API void dispatch(VkCommandBuffer cmd, uint32_t workItemsX, uint32_t workItemsY = 1,
                                   uint32_t workItemsZ = 1);

        /// <summary>
        /// Dispatch a number of workgroups, without taking the local size of the shader into account.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="groupCountX">Number of workgroups in x</param>
        /// <param name="groupCountY">Number of workgroups in y</param>
        /// <param name="groupCountZ">Number of workgroups in z</param>
        MANDRILL_API void dispatchGroups(VkCommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY = 1,
                                         uint32_t groupCountZ = 1);

        /// <summary>
        /// Transition an image for writing to from the compute shader.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="image">Image to transition</param>
        MANDRILL_API void write(VkCommandBuffer cmd, VkImage image);

        /// <summary>
        /// Transition an image that the compute shader wrote to for reading.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="image">Image to transition</param>
        MANDRILL_API void read(VkCommandBuffer cmd, VkImage image);

        /// <summary>
        /// Get the local workgroup size of the shader, as reflected from the SPIR-V.
        /// </summary>
        /// <returns>Local workgroup size</returns>
        MANDRILL_API glm::uvec3 getLocalSize() const
        {
            return mLocalSize;
        }

    private:
        void createPipeline() override;

        VkPipelineCreateFlags mFlags;

        // Local size of the shader, so that dispatch() can turn work items into workgroups
        glm::uvec3 mLocalSize;
    };
} // namespace Mandrill
