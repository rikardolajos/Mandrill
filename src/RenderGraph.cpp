#include "RenderGraph.h"

#include "Helpers.h"
#include "Log.h"

using namespace Mandrill;

RenderGraph::~RenderGraph()
{
    for (auto& semaphore : mSemaphores) {
        vkDestroySemaphore(mpDevice->getDevice(), semaphore, nullptr);
    }
}

void RenderGraph::addResource(const std::string& name, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage,
                              VkImageLayout initialLayout, VkImageLayout finalLayout)
{
    Resource resource = {
        .name = name,
        .format = format,
        .extent = extent,
        .usage = usage,
        .initialLayout = initialLayout,
        .finalLayout = finalLayout,
    };
    mResources[name] = resource;
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
}

void RenderGraph::compile()
{
    // Dependecy Graph Construction
    // Build bidirectional dependency graph between passes
    std::vector<std::vector<uint32_t>> passDependencies(mPasses.size()); // What each pass depends on
    std::vector<std::vector<uint32_t>> passDependents(mPasses.size());   // What depends on each pass

    // Track which pass produces each resource (write-after-write dependencies)
    std::unordered_map<std::string, uint32_t> resourceWriters;

    // Dependency Discovery Through Resource Usage Analysis
    // Analyze each pass to determine data flow relationships
    for (uint32_t i = 0; i < count(mPasses); i++) {
        const auto& pass = mPasses[i];

        // Process input dependencies - this pass must wait for producers
        for (const auto& input : pass.inputs) {
            auto it = resourceWriters.find(input);
            if (it != resourceWriters.end()) {
                // Found the pass that produces this input - create dependency link
                uint32_t producerIndex = it->second;
                passDependencies[i].push_back(producerIndex); // This pass depends on the producer
                passDependents[producerIndex].push_back(i);   // Produces has this dependent
            }
        }

        // Register output production - subsequent passes may depend on these
        for (const auto& output : pass.outputs) {
            resourceWriters[output] = i; // Record this pass as producer
        }
    }

    // Topological Sort for Optimal Execution Order
    // Use depth-first search to compute valid execution sequence while detechting cycles
    std::vector<bool> visited(mPasses.size(), false); // Track completed nodes
    std::vector<bool> inStack(mPasses.size(), false); // Track current recursion path

    std::function<void(uint32_t)> visit = [&](uint32_t node) {
        if (inStack[node]) {
            Log::Error("Cycle detected in render graph dependencies");
        }

        if (visited[node]) {
            return; // Already processed this node
        }

        inStack[node] = true; // Mark current path

        for (uint32_t dep : passDependencies[node]) {
            visit(dep); // Visit dependencies first
        }

        inStack[node] = false; // Unmark path
        visited[node] = true;  // Mark as completed
        executionOrder.push_back(node); // Add to execution order
    };

    // Visit all nodes to ensure all passes are included in the execution order
    for (uint32_t i = 0; i < count(mPasses); i++) {
        if (!visited[i]) {
            visit(i);
        }
    }

    // Automatic Synchronization Object Creation
    // Generate semaphores for all dependencies indentified during analysis
    for (uint32_t i = 0; i < count(mPasses); i++) {
        for (auto dep : passDependencies[i]) {
            // Create a GPU semaphore for this dependency relationship
            // The dependent pass will wait on this semaphore before executing
            VkSemaphore semaphore;
            VkSemaphoreCreateInfo ci = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
            };
            vkCreateSemaphore(mpDevice->getDevice(), &ci, nullptr, &semaphore);
            mSemaphores.emplace_back(semaphore);
            mSemaphoreSignalWaitPairs.emplace_back(dep, i); // (producer, consumer) pair
        }
    }

    // Physical Resource Allocation and Creation
    // Transform resource descriptions into actual GPU objects
    for (auto& [name, resource] : mResources) {
        resource.pImage = make_ptr<Image>(mpDevice, resource.extent.width, resource.extent.height, 1, 1, VK_SAMPLE_COUNT_1_BIT,
                                        resource.format, VK_IMAGE_TILING_OPTIMAL, resource.usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        resource.pImage->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void RenderGraph::execute(VkCommandBuffer cmd)
{
    // Execution state management for dynamic synchronization
    std::vector<VkCommandBuffer> cmdBuffers;      // Command buffer storage
    std::vector<VkSemaphore> waitSemaphores;      // Synchronization dependencies for current pass
    std::vector<VkPipelineStageFlags> waitStages; // Pipeline stages to wait on
    std::vector<VkSemaphore> signalSemaphores;    // Semaphores to signal after current pass

    // Ordered Pass Execution with Automatic Dependency Management
    // Execute each pass in the computed dependency-safe order
    for (auto passIdx : executionOrder) {
        const auto& pass = mPasses[passIdx];

        // Synchronization Setup - Collect Dependencies for Current Pass
        // Determine what this pass must wait for before executing
        waitSemaphores.clear();
        waitStages.clear();

        for (size_t i = 0; i < mSemaphoreSignalWaitPairs.size(); ++i) {
            if (mSemaphoreSignalWaitPairs[i].second == passIdx) {
                // This pass depends on the completion of another pass
                waitSemaphores.push_back(mSemaphores[i]); // Wait for dependency completion
                waitStages.push_back(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT); // Wait at output stage
            }
        }

        // Collect semaphores that this pass will signal for dependent passes
        signalSemaphores.clear();
        for (size_t i = 0; i < mSemaphoreSignalWaitPairs.size(); ++i) {
            if (mSemaphoreSignalWaitPairs[i].first == passIdx) {
                // Other passes depend on this pass's completion
                signalSemaphores.push_back(mSemaphores[i]); // Signal completion for dependents
            }
        }

        // TODO
        // Command Buffer Preparation and Resource Layout Transitions
        // Set up command recording and transition resources to appropriate layouts
        //commandBuffer.begin({}); // Begin command recording

        // Transition input resources to shader-readable layouts
        for (const auto& input : pass.inputs) {
            auto& resource = mResources[input];

            Helpers::imageBarrier(cmd, resource.pImage->getImage(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT,
                                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                  resource.initialLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        // Transition output resources to render target layouts
        for (const auto& output : pass.outputs) {
            auto& resource = mResources[output];

            Helpers::imageBarrier(cmd, resource.pImage->getImage(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, resource.initialLayout,
                                  VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
        }

        // Pass Execution - Execute the Actual Rendering Logic
        // Call the user-provided rendering function with prepared command buffer
        pass.executeFunc(cmd); // Execute pass-specific rendering

        // Final Layout Transitions - Prepare Resources for Subsequent Use
        // Transition output resources to their final required layouts
        for (const auto& output : pass.outputs) {
            auto& resource = mResources[output];

            Helpers::imageBarrier(cmd, resource.pImage->getImage(), VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                  VK_ACCESS_2_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                  resource.finalLayout);
        }

        // TODO
        // Command Submission with Synchronization
        // Submit command buffer with appropriate dependency and signaling semaphores
        //commandBuffer.end(); // Finalize command recording

        //vk::SubmitInfo submitInfo;
        //submitInfo
        //    .setWaitSemaphoreCount(static_cast<uint32_t>(waitSemaphores.size()))     // Dependencies to wait for
        //    .setPWaitSemaphores(waitSemaphores.data())                               // Dependency semaphores
        //    .setPWaitDstStageMask(waitStages.data())                                 // Pipeline stages to wait at
        //    .setCommandBufferCount(1)                                                // Single command buffer
        //    .setPCommandBuffers(&*commandBuffer)                                     // Command buffer to execute
        //    .setSignalSemaphoreCount(static_cast<uint32_t>(signalSemaphores.size())) // Semaphores to signal
        //    .setPSignalSemaphores(signalSemaphores.data());                          // Signal semaphores

        //queue.submit(1, &submitInfo, nullptr); // Submit to GPU queue
    }
}
