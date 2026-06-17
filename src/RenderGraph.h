#pragma once

#include "Common.h"

#include "Device.h"
#include "Image.h"

namespace Mandrill
{
    class RenderGraph
    {
    public:
        MANDRILL_NON_COPYABLE(RenderGraph)

        /// <summary>
        /// Create a render graph that will manage the resources and execution of a series of passes. The render graph
        /// will handle the synchronization between passes and the transitions of resources.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        MANDRILL_API explicit RenderGraph(ptr<Device> pDevice) : mpDevice(pDevice){};

        /// <summary>
        /// Destructor for RenderGraph.
        /// </summary>
        MANDRILL_API ~RenderGraph();

        /// <summary>
        /// Add resource to the render graph.
        /// </summary>
        /// <param name="name">Name of resource</param>
        /// <param name="format">Format of the resource</param>
        /// <param name="extent">Extent of the resource</param>
        /// <param name="usage">Usage flags for the resource</param>
        /// <param name="initialLayout">Initial layout of the resource</param>
        /// <param name="finalLayout">Final layout of the resource</param>
        MANDRILL_API void addResource(const std::string& name, VkFormat format, VkExtent2D extent,
                                      VkImageUsageFlags usage, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                      VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        /// <summary>
        /// Add a pass to the render graph. The pass will be executed in the order it was added.
        /// </summary>
        /// <param name="name">Name of the pass</param>
        /// <param name="inputs">Input resources for the pass</param>
        /// <param name="outputs">Output resources for the pass</param>
        /// <param name="executeFunc">Function to execute the pass</param>
        MANDRILL_API void addPass(const std::string& name, const std::vector<std::string>& inputs,
                                  const std::vector<std::string>& outputs,
                                  std::function<void(VkCommandBuffer cmd)> executeFunc);

        /// <summary>
        /// Compile the render graph. This will prepare the resources and passes for execution.
        /// </summary>
        MANDRILL_API void compile();

        /// <summary>
        /// Execute the render graph. This will execute all passes in the order determined by the render graph.
        /// </summary>
        /// <param name="cmd">Command buffer to use for execution</param>
        MANDRILL_API void execute(VkCommandBuffer cmd);

        /// <summary>
        /// Get resource from render graph by name.
        /// </summary>
        /// <param name="name">Name of resource</param>
        /// <returns>Pointer to the resource image, or nullptr if not found</returns>
        MANDRILL_API ptr<Image> getResource(const std::string& name) const
        {
            auto it = mResources.find(name);
            if (it != mResources.end()) {
                return it->second.pImage;
            }
            Log::Error("Resource not found in render graph: {}", name);
            return nullptr;
        }

    private:
        struct Resource {
            std::string name;
            VkFormat format;
            VkExtent2D extent;
            VkImageUsageFlags usage;
            VkImageLayout initialLayout;
            VkImageLayout finalLayout;

            ptr<Image> pImage = nullptr;
        };

        struct Pass {
            std::string name;
            std::vector<std::string> inputs;
            std::vector<std::string> outputs;
            std::function<void(VkCommandBuffer cmd)> executeFunc;
        };

        std::unordered_map<std::string, Resource> mResources;
        std::vector<Pass> mPasses;
        std::vector<uint32_t> executionOrder;

        std::vector<VkSemaphore> mSemaphores;
        std::vector<std::pair<uint32_t, uint32_t>> mSemaphoreSignalWaitPairs;

        ptr<Device> mpDevice;
    };
} // namespace Mandrill
