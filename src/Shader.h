#pragma once

#include "Common.h"

#include "Descriptor.h"
#include "Device.h"

#include <deque>

namespace Mandrill
{
    struct ShaderDesc {
        /// <summary>
        /// Path to shader source code
        /// </summary>
        std::filesystem::path filename;

        /// <summary>
        /// Entry point of shader (typically "main")
        /// </summary>
        std::string entry;

        /// <summary>
        /// Shader stage
        /// </summary>
        VkShaderStageFlagBits stageFlags;

        /// <summary>
        /// Optional specialization info constants
        /// </summary>
        VkSpecializationInfo* pSpecializationInfo;

        /// <summary>
        /// Shader description constructor, used to create a shader module.
        /// </summary>
        /// <param name="filename">Path to shader source code</param>
        /// <param name="entry">Entry point of shader (typically "main")</param>
        /// <param name="stageFlags">Shader stage</param>
        /// <param name="pSpecializationInfo">Optional setup for specialization constants</param>
        MANDRILL_API ShaderDesc(std::filesystem::path filename, std::string entry, VkShaderStageFlagBits stageFlags,
                                VkSpecializationInfo* pSpecializationInfo = nullptr)
            : filename(filename), entry(entry), stageFlags(stageFlags), pSpecializationInfo(pSpecializationInfo)
        {
        }
    };

    /// <summary>
    /// Shader class that abstracts the handling of Vulkan shaders. This class manages shader modules and hot reloading
    /// of shader source code during execution.
    ///
    /// Resources can be attached by the name they have in the shader, using setResource(). Which set and binding they
    /// belong to, and which descriptor type they are, is taken from the shader reflection, so the application never
    /// has to describe a descriptor. Call bindResources() to bind them for a draw or dispatch.
    ///
    /// setResource() attaches a resource, it is not a per-frame update. Data that varies per frame or per mesh
    /// belongs in a single buffer that is attached once and bound repeatedly with a dynamic offset, which is what
    /// the bindResources() overload taking a set and offsets is for.
    /// </summary>
    class Shader
    {
    public:
        MANDRILL_NON_COPYABLE(Shader)

        /// <summary>
        /// Where a named resource lives in the shader, as reflected from the SPIR-V.
        /// </summary>
        struct ResourceInfo {
            /// <summary> Descriptor set the resource belongs to </summary>
            uint32_t set;
            /// <summary> Binding number within the set </summary>
            uint32_t binding;
            /// <summary> Descriptor type reflected for the binding </summary>
            VkDescriptorType type;
            /// <summary> Number of descriptors, greater than one for arrays </summary>
            uint32_t count;
        };

        /// <summary>
        /// Create a new shader.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="desc">Description of shader being created</param>
        /// <param name="pushDescriptorSets">Sets that should use push descriptors, which are written straight into
        /// the command buffer instead of being allocated from a pool. A set can only do this if it holds no dynamic
        /// descriptors and fits within the device limit, and it can then no longer be allocated from, so only list
        /// sets whose resources are attached with setResource().</param>
        MANDRILL_API Shader(ptr<Device> pDevice, const std::vector<ShaderDesc>& desc,
                            const std::vector<uint32_t>& pushDescriptorSets = {});

        /// <summary>
        /// Destructor for shader.
        /// </summary>
        MANDRILL_API ~Shader();

        /// <summary>
        /// Reload shader code from disk and recompile it. Resources attached with setResource() are kept and are
        /// resolved against the new reflection, so they do not have to be set again.
        /// </summary>
        MANDRILL_API void reload();

        /// <summary>
        /// Attach a buffer to a named resource in the shader. Whether it ends up as a uniform buffer or a storage
        /// buffer, and where it is bound, follows from the shader.
        /// </summary>
        /// <param name="name">Name of the resource in the shader. Either the variable name, or the block type name
        /// for blocks that are declared without an instance name.</param>
        /// <param name="pBuffer">Buffer to attach</param>
        /// <param name="offset">Offset into the buffer</param>
        /// <param name="range">Range of the buffer to expose</param>
        MANDRILL_API void setResource(const std::string& name, ptr<Buffer> pBuffer, VkDeviceSize offset = 0,
                                      VkDeviceSize range = VK_WHOLE_SIZE);

        /// <summary>
        /// Attach a texture to a named combined image sampler in the shader.
        /// </summary>
        /// <param name="name">Name of the resource in the shader</param>
        /// <param name="pTexture">Texture to attach</param>
        MANDRILL_API void setResource(const std::string& name, ptr<Texture> pTexture);

        /// <summary>
        /// Attach an array of textures to a named combined image sampler array in the shader.
        /// </summary>
        /// <param name="name">Name of the resource in the shader</param>
        /// <param name="textures">Textures to attach</param>
        MANDRILL_API void setResource(const std::string& name, const std::vector<ptr<Texture>>& textures);

        /// <summary>
        /// Attach an image to a named storage image or input attachment in the shader.
        /// </summary>
        /// <param name="name">Name of the resource in the shader</param>
        /// <param name="pImage">Image to attach</param>
        /// <param name="layout">Layout the image will be in when accessed. Defaults to the layout implied by the
        /// descriptor type.</param>
        MANDRILL_API void setResource(const std::string& name, ptr<Image> pImage,
                                      VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED);

        /// <summary>
        /// Attach an acceleration structure to a named resource in the shader.
        /// </summary>
        /// <param name="name">Name of the resource in the shader</param>
        /// <param name="pAccelerationStructure">Acceleration structure to attach</param>
        MANDRILL_API void setResource(const std::string& name, ptr<AccelerationStructure> pAccelerationStructure);

        /// <summary>
        /// Bind every descriptor set that has resources attached to it. Sets that contain dynamic descriptors need
        /// an offset and should be bound with the overload that takes one.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="bindPoint">Bind point in pipeline</param>
        MANDRILL_API void bindResources(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint);

        /// <summary>
        /// Bind one descriptor set. Use this for per-frame or per-mesh sets, where the same set is bound several
        /// times with different dynamic offsets.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="bindPoint">Bind point in pipeline</param>
        /// <param name="set">Set to bind</param>
        /// <param name="dynamicOffsets">Offsets for the dynamic descriptors in the set, in binding order</param>
        MANDRILL_API void bindResources(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, uint32_t set,
                                        const std::vector<uint32_t>& dynamicOffsets = {});

        /// <summary>
        /// Check whether the shader has a resource with a given name.
        /// </summary>
        /// <param name="name">Name of the resource in the shader</param>
        /// <returns>True if the shader declares it</returns>
        MANDRILL_API bool hasResource(const std::string& name) const
        {
            return mResourceInfos.count(name) > 0;
        }

        /// <summary>
        /// Get where a named resource lives in the shader.
        /// </summary>
        /// <param name="name">Name of the resource in the shader</param>
        /// <returns>Reflected information, or nothing if the shader does not declare it</returns>
        MANDRILL_API std::optional<ResourceInfo> getResourceInfo(const std::string& name) const
        {
            auto found = mResourceInfos.find(name);
            return found == mResourceInfos.end() ? std::nullopt : std::optional<ResourceInfo>(found->second);
        }

        /// <summary>
        /// Get shader module handles.
        /// </summary>
        /// <returns>Vector of shader module handles</returns>
        MANDRILL_API std::vector<VkShaderModule> getModules() const
        {
            return mModules;
        }

        /// <summary>
        /// Get pipeline shader stage create infos.
        /// </summary>
        /// <returns>Vector of pipeline shader stage create infos</returns>
        MANDRILL_API std::vector<VkPipelineShaderStageCreateInfo> getStages() const
        {
            return mStages;
        }

        /// <summary>
        /// Get the descriptor set layout of one set.
        /// </summary>
        /// <returns>Vector of descriptor set layouts</returns>
        MANDRILL_API VkDescriptorSetLayout getDescriptorSetLayout(uint32_t set) const
        {
            return mDescriptorSetLayouts[set];
        }

        /// <summary>
        /// Get the descriptor set layouts.
        /// </summary>
        /// <returns>Vector of descriptor set layouts</returns>
        MANDRILL_API const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts() const
        {
            return mDescriptorSetLayouts;
        }

        /// <summary>
        /// Get the push constant ranges.
        /// </summary>
        /// <returns>Vector of push constant ranges</returns>
        MANDRILL_API const std::vector<VkPushConstantRange>& getPushConstantRanges() const
        {
            return mPushConstantRanges;
        }

        /// <summary>
        /// Get tthe pipeline layout.
        /// </summary>
        /// <returns>Pipeline layout</returns>
        MANDRILL_API VkPipelineLayout getPipelineLayout() const
        {
            return mPipelineLayout;
        }

        /// <summary>
        /// Check whether a set uses push descriptors.
        /// </summary>
        /// <param name="set">Set to query</param>
        /// <returns>True if the set is pushed rather than allocated</returns>
        MANDRILL_API bool isPushDescriptorSet(uint32_t set) const
        {
            return set < mSetIsPush.size() && mSetIsPush[set];
        }

    private:
        // A resource attached to the shader. Only the members matching the reflected descriptor type are used.
        struct BoundResource {
            ptr<Buffer> pBuffer;
            VkDeviceSize offset = 0;
            VkDeviceSize range = VK_WHOLE_SIZE;

            ptr<Texture> pTexture;
            std::vector<ptr<Texture>> textures;

            ptr<Image> pImage;
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            ptr<AccelerationStructure> pAccelerationStructure;
        };

        // Backing storage for a batch of descriptor writes. The writes hold pointers into these, and a deque never
        // moves what it already holds.
        struct DescriptorWrites {
            std::vector<VkWriteDescriptorSet> writes;
            std::deque<VkDescriptorBufferInfo> buffers;
            std::deque<std::vector<VkDescriptorImageInfo>> images;
            std::deque<VkWriteDescriptorSetAccelerationStructureKHR> accelerationStructureInfos;
            std::deque<VkAccelerationStructureKHR> accelerationStructures;
        };

        void createShader();
        void createDescriptorPool();
        void destroyDescriptorPools();
        void buildWrites(uint32_t set, VkDescriptorSet dstSet, DescriptorWrites& out) const;

        // Looks up a resource and checks that the shader expects the kind of resource being attached
        const ResourceInfo* resolveResource(const std::string& name, const char* kind,
                                            std::initializer_list<VkDescriptorType> allowed);

        VkDescriptorSet allocateDescriptorSet(uint32_t set);
        void updateDescriptorSet(uint32_t set);

        ptr<Device> mpDevice;

        std::vector<VkShaderModule> mModules;
        std::vector<ptr<spv_reflect::ShaderModule>> mReflections;
        std::vector<VkPipelineShaderStageCreateInfo> mStages;

        std::vector<std::string> mEntries;
        std::vector<std::filesystem::path> mSrcFilenames;
        std::vector<VkShaderStageFlagBits> mStageFlags;
        std::vector<VkSpecializationInfo*> mSpecializationInfos;

        std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts; // One per set
        std::vector<VkPushConstantRange> mPushConstantRanges;
        VkPipelineLayout mPipelineLayout;

        // Resources are kept keyed on name so that they survive a reload, where set and binding could change
        std::map<std::string, ResourceInfo> mResourceInfos;
        std::map<std::string, BoundResource> mResources;

        std::vector<uint32_t> mRequestedPushSets;     // Sets the application asked to be pushed
        std::vector<bool> mSetIsPush;                 // Sets that ended up eligible and are pushed
        std::vector<VkDescriptorSet> mDescriptorSets; // One per set, the currently written set
        std::vector<bool> mSetDirty;                  // Whether the set has to be written before the next bind

        // Sets are never written in place, a new one is taken from the pool instead, so that a set still being read
        // by a frame in flight is left untouched. Pools are only reclaimed when the shader is destroyed or reloaded.
        std::vector<VkDescriptorPool> mDescriptorPools;
        std::vector<VkDescriptorPoolSize> mPoolSizes;
        uint32_t mMaxSetsPerPool = 0;
    };
} // namespace Mandrill
