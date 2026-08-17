#include "RenderGraph.h"

#include "Helpers.h"
#include "Log.h"

using namespace Mandrill;

namespace
{
    bool isDepthFormat(VkFormat format)
    {
        switch (format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }

    bool hasStencilComponent(VkFormat format)
    {
        switch (format) {
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }

    // Aspects an image view of the format covers. A depth-stencil image is viewed as depth only, which is what an
    // attachment expects.
    VkImageAspectFlags viewAspect(VkFormat format)
    {
        return isDepthFormat(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    }

    // Aspects a barrier for the format has to cover, which unlike the view is all of them
    VkImageAspectFlags barrierAspect(VkFormat format)
    {
        if (!isDepthFormat(format)) {
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
        return VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencilComponent(format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
    }

    // Layout a pass needs the resource in to write it
    VkImageLayout writeLayout(VkImageUsageFlags usage)
    {
        if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        return VK_IMAGE_LAYOUT_GENERAL;
    }

    // Layout a pass needs the resource in to read it
    VkImageLayout readLayout(VkImageUsageFlags usage)
    {
        if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        if (usage & VK_IMAGE_USAGE_STORAGE_BIT) {
            return VK_IMAGE_LAYOUT_GENERAL;
        }
        if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        }
        return VK_IMAGE_LAYOUT_GENERAL;
    }

    struct Sync {
        VkPipelineStageFlags2 stage;
        VkAccessFlags2 access;
    };

    // How an image in a given layout is accessed, which is what a barrier to or from that layout has to synchronize
    Sync syncOfLayout(VkImageLayout layout)
    {
        switch (layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        case VK_IMAGE_LAYOUT_GENERAL:
            return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return {VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return {VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
        case VK_IMAGE_LAYOUT_UNDEFINED:
        default:
            // Nothing to make available, the contents are being discarded
            return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_NONE};
        }
    }
} // namespace

void RenderGraph::addResource(const std::string& name, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage,
                              VkImageLayout finalLayout)
{
    Resource resource = {
        .name = name,
        .format = format,
        .extent = extent,
        .usage = usage,
        .finalLayout = finalLayout,
    };
    mResources[name] = resource;

    // The resource has to be created again before the graph can run
    mCompiled = false;
}

void RenderGraph::addPass(const std::string& name, const std::vector<std::string>& inputs,
                          const std::vector<std::string>& outputs, std::function<void(VkCommandBuffer cmd)> executeFunc)
{
    Pass pass = {
        .name = name,
        .inputs = inputs,
        .outputs = outputs,
        .executeFunc = executeFunc,
    };
    mPasses.push_back(pass);

    mCompiled = false;
}

void RenderGraph::compile()
{
    mCompiled = false;
    mPassOrder.clear();
    mExecutionOrder.clear();

    if (!validate()) {
        return;
    }

    if (!sortPasses()) {
        mPassOrder.clear();
        mExecutionOrder.clear();
        return;
    }

    createResources();

    mCompiled = true;
}

void RenderGraph::execute(VkCommandBuffer cmd)
{
    if (!mCompiled) {
        // compile() reported why, no point in repeating it once a frame
        return;
    }

    for (auto passIndex : mPassOrder) {
        const auto& pass = mPasses[passIndex];

        // A barrier is emitted for every resource the pass touches, also when the layout already is the one the pass
        // needs, since it is the barrier that makes the previous pass's writes visible to this one
        for (const auto& input : pass.inputs) {
            auto& resource = mResources[input];
            transitionResource(cmd, resource, readLayout(resource.usage));
        }

        for (const auto& output : pass.outputs) {
            auto& resource = mResources[output];
            transitionResource(cmd, resource, writeLayout(resource.usage));
        }

        pass.executeFunc(cmd);
    }

    // Hand the resources that are consumed outside of the graph over in the layout they are expected in
    for (auto& [name, resource] : mResources) {
        if (resource.finalLayout != VK_IMAGE_LAYOUT_UNDEFINED) {
            transitionResource(cmd, resource, resource.finalLayout);
        }
    }
}

bool RenderGraph::validate() const
{
    bool valid = true;

    // Which pass produces a resource is what the whole ordering rests on, so a resource can only have one producer.
    // Otherwise the order between the two would be whatever order they happened to be added in.
    std::unordered_map<std::string, std::string> resourceWriter;

    for (const auto& pass : mPasses) {
        for (const auto& input : pass.inputs) {
            if (!mResources.count(input)) {
                Log::Error("Pass {} reads resource {} which is not in the render graph", pass.name, input);
                valid = false;
            }
        }

        for (const auto& output : pass.outputs) {
            if (!mResources.count(output)) {
                Log::Error("Pass {} writes resource {} which is not in the render graph", pass.name, output);
                valid = false;
            }

            auto writer = resourceWriter.find(output);
            if (writer != resourceWriter.end()) {
                Log::Error("Resource {} is written by both pass {} and pass {}, but a render graph resource can only "
                           "be written by one pass",
                           output, writer->second, pass.name);
                valid = false;
            } else {
                resourceWriter[output] = pass.name;
            }
        }
    }

    for (const auto& pass : mPasses) {
        for (const auto& input : pass.inputs) {
            if (!resourceWriter.count(input)) {
                Log::Warning("Pass {} reads resource {}, which no pass writes", pass.name, input);
            }
        }
    }

    return valid;
}

bool RenderGraph::sortPasses()
{
    // Build the dependency graph between passes from how they use the resources. Since a resource has a single
    // producer, a pass reading it having to run after that producer is the only ordering there is, and the order the
    // passes were added in does not come into it.
    std::vector<std::vector<uint32_t>> passDependencies(mPasses.size());

    std::unordered_map<std::string, uint32_t> resourceWriter;

    for (uint32_t i = 0; i < count(mPasses); i++) {
        for (const auto& output : mPasses[i].outputs) {
            resourceWriter[output] = i;
        }
    }

    for (uint32_t i = 0; i < count(mPasses); i++) {
        for (const auto& input : mPasses[i].inputs) {
            auto writer = resourceWriter.find(input);
            if (writer != resourceWriter.end() && writer->second != i) {
                passDependencies[i].push_back(writer->second);
            }
        }
    }

    // Depth-first topological sort, which puts a pass after everything it depends on
    std::vector<bool> visited(mPasses.size(), false); // Nodes that are done and already in the order
    std::vector<bool> inStack(mPasses.size(), false); // Nodes on the current path, to catch cycles

    bool cycle = false;

    std::function<void(uint32_t)> visit = [&](uint32_t node) {
        if (visited[node] || cycle) {
            return;
        }

        if (inStack[node]) {
            Log::Error("Cycle detected in render graph dependencies, at pass {}", mPasses[node].name);
            cycle = true;
            return;
        }

        inStack[node] = true;

        for (uint32_t dependency : passDependencies[node]) {
            visit(dependency);
        }

        inStack[node] = false;
        visited[node] = true;

        mPassOrder.push_back(node);
    };

    for (uint32_t i = 0; i < count(mPasses); i++) {
        visit(i);
    }

    if (cycle) {
        return false;
    }

    for (auto passIndex : mPassOrder) {
        mExecutionOrder.push_back(mPasses[passIndex].name);
    }

    return true;
}

void RenderGraph::createResources()
{
    for (auto& [name, resource] : mResources) {
        // Release the old image first, so that a recompile does not hold two sets of resources at once
        resource.pImage = nullptr;

        resource.pImage =
            make_ptr<Image>(mpDevice, resource.extent.width, resource.extent.height, 1, 1, VK_SAMPLE_COUNT_1_BIT,
                            resource.format, VK_IMAGE_TILING_OPTIMAL, resource.usage,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        resource.pImage->createImageView(viewAspect(resource.format));

        resource.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

void RenderGraph::transitionResource(VkCommandBuffer cmd, Resource& resource, VkImageLayout newLayout) const
{
    Sync src = syncOfLayout(resource.currentLayout);
    Sync dst = syncOfLayout(newLayout);

    VkImageSubresourceRange subresourceRange = {
        .aspectMask = barrierAspect(resource.format),
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    Helpers::imageBarrier(cmd, resource.pImage->getImage(), src.stage, src.access, dst.stage, dst.access,
                          resource.currentLayout, newLayout, &subresourceRange);

    resource.currentLayout = newLayout;
}
