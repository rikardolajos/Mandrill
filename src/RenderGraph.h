#pragma once

#include "Common.h"

#include "Device.h"
#include "Image.h"

namespace Mandrill
{
    /// <summary>
    /// Render graph that owns the images a frame is built from and works out in which order the passes that read and
    /// write them have to run. A pass is added with the resources it reads and the resources it writes, so passes can
    /// be added in any order and the graph derives the execution order from the data flow between them. Every layout
    /// transition, and the barrier that makes one pass's writes visible to the next pass's reads, is emitted by the
    /// graph. Since it is the data flow that decides the order, a resource can only be written by one pass, and
    /// compile() reports it if two passes write the same one.
    ///
    /// Which layout a resource is transitioned to follows from the usage flags it was added with: a resource being
    /// written goes to the attachment layout if it can be an attachment and to general otherwise, and a resource being
    /// read goes to the shader-read-only layout if it can be sampled and to general otherwise. A G-buffer attachment
    /// that a later pass reads as a storage image is therefore added with both usage flags.
    ///
    /// The graph only manages resources and synchronization. Beginning and ending rendering is left to the pass
    /// function, so an application creates its own Pass objects from the images that getResource() returns.
    /// </summary>
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
        /// Add a resource to the render graph. Adding a resource that is already in the graph replaces it, which is how
        /// a graph is given a new extent after the window was resized: add the resources again and compile again.
        /// </summary>
        /// <param name="name">Name of resource</param>
        /// <param name="format">Format of the resource</param>
        /// <param name="extent">Extent of the resource</param>
        /// <param name="usage">Usage flags for the resource, which also decide the layouts it is transitioned to</param>
        /// <param name="finalLayout">Layout to leave the resource in once every pass has run, for a resource that is
        /// consumed outside the graph. Defaults to leaving it in the layout the last pass used it in.</param>
        MANDRILL_API void addResource(const std::string& name, VkFormat format, VkExtent2D extent,
                                      VkImageUsageFlags usage, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED);

        /// <summary>
        /// Add a pass to the render graph. The order passes are added in does not matter, compile() orders them after
        /// the resources they read and write.
        /// </summary>
        /// <param name="name">Name of the pass</param>
        /// <param name="inputs">Resources the pass reads</param>
        /// <param name="outputs">Resources the pass writes</param>
        /// <param name="executeFunc">Function to execute the pass</param>
        MANDRILL_API void addPass(const std::string& name, const std::vector<std::string>& inputs,
                                  const std::vector<std::string>& outputs,
                                  std::function<void(VkCommandBuffer cmd)> executeFunc);

        /// <summary>
        /// Compile the render graph. This resolves the execution order of the passes and creates the resources, and has
        /// to be called after the passes and resources have been added and before the first execute(). Compiling again
        /// recreates the resources, which is what a resize needs, and keeps the passes that were already added.
        /// </summary>
        MANDRILL_API void compile();

        /// <summary>
        /// Execute the render graph by recording every pass, in the order compile() resolved, into a command buffer.
        /// The graph does not submit anything, so pass the command buffer of the current frame.
        /// </summary>
        /// <param name="cmd">Command buffer to use for execution</param>
        MANDRILL_API void execute(VkCommandBuffer cmd);

        /// <summary>
        /// Get resource from render graph by name. The image only exists once the graph has been compiled.
        /// </summary>
        /// <param name="name">Name of resource</param>
        /// <returns>Pointer to the resource image, or nullptr if not found</returns>
        MANDRILL_API ptr<Image> getResource(const std::string& name) const
        {
            auto it = mResources.find(name);
            if (it == mResources.end()) {
                Log::Error("Resource not found in render graph: {}", name);
                return nullptr;
            }
            if (!it->second.pImage) {
                Log::Error("Resource {} has no image yet, since the render graph has not compiled", name);
            }
            return it->second.pImage;
        }

        /// <summary>
        /// Get the names of the passes in the order compile() resolved them to. Empty if the graph has not compiled.
        /// </summary>
        /// <returns>Pass names in execution order</returns>
        MANDRILL_API const std::vector<std::string>& getExecutionOrder() const
        {
            return mExecutionOrder;
        }

    private:
        struct Resource {
            std::string name;
            VkFormat format;
            VkExtent2D extent;
            VkImageUsageFlags usage;
            VkImageLayout finalLayout;

            ptr<Image> pImage = nullptr;

            // Layout the image is in at this point of the recording. Kept across frames, since the image is not
            // recreated between them, so the first pass of a frame transitions from what the last frame left behind.
            VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        };

        struct Pass {
            std::string name;
            std::vector<std::string> inputs;
            std::vector<std::string> outputs;
            std::function<void(VkCommandBuffer cmd)> executeFunc;
        };

        // Checks that every pass only names resources that were added to the graph
        bool validate() const;

        // Orders the passes after the data flow between them, leaving the result in mExecutionOrder
        bool sortPasses();

        void createResources();

        void transitionResource(VkCommandBuffer cmd, Resource& resource, VkImageLayout newLayout) const;

        std::unordered_map<std::string, Resource> mResources;
        std::vector<Pass> mPasses;

        std::vector<uint32_t> mPassOrder;             // Indices into mPasses
        std::vector<std::string> mExecutionOrder;     // The same order, by name, for the application to inspect
        bool mCompiled = false;

        ptr<Device> mpDevice;
    };
} // namespace Mandrill
