#include "Deferred.h"

#include "Error.h"
#include "Helpers.h"
#include "Scene.h"

using namespace Mandrill;

enum {
    GBUFFER_PASS = 0,
    RESOLVE_PASS = 1,
};

static VkVertexInputBindingDescription bindingDescription = {
    .binding = 0,
    .stride = sizeof(Vertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
};

static std::array<VkVertexInputAttributeDescription, 5> attributeDescription = {{
    {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, position),
    },
    {
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, normal),
    },
    {
        .location = 2,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, tangent),
    },
    {
        .location = 3,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, binormal),
    },
    {
        .location = 4,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, texcoord),
    },
}};

static void createGBufferPipeline()
{
}


Deferred::Deferred(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                   const RenderPassDescription& desc)
    : RenderPass(pDevice, pSwapchain, desc)
{
    if (mpLayouts.size() != 2 or mpShaders.size() != 2) {
        Log::error("Deferred rendering only supports two render passes and was created with wrong number of layouts "
                   "and shaders");
    }

    createAttachments();
    createRenderPass();
    createFramebuffers();
    createPipelines();
}

Deferred::~Deferred()
{
    vkDeviceWaitIdle(mpDevice->getDevice());
    destroyPipelines();
    destroyFramebuffers();
    vkDestroyRenderPass(mpDevice->getDevice(), mRenderPass, nullptr);
    destroyAttachments();
}

void Deferred::createPipelines()
{
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size()),
        .pVertexAttributeDescriptions = attributeDescription.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_CULL_MODE,
        VK_DYNAMIC_STATE_FRONT_FACE,
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    std::array<VkPipelineDepthStencilStateCreateInfo, 2> depthStencils = {};
    depthStencils[GBUFFER_PASS] = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };
    depthStencils[RESOLVE_PASS] = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    std::array<VkPipelineColorBlendAttachmentState, 3> colorBlendAttachmentStates = {
        colorBlendAttachment,
        colorBlendAttachment,
        colorBlendAttachment,
    };

    std::array<VkPipelineColorBlendStateCreateInfo, 2> colorBlending = {};
    colorBlending[GBUFFER_PASS] = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = static_cast<uint32_t>(colorBlendAttachmentStates.size()),
        .pAttachments = colorBlendAttachmentStates.data(),
    };
    colorBlending[RESOLVE_PASS] = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    std::array<VkGraphicsPipelineCreateInfo, 2> cis = {};

    // Create info for G-buffer pipeline
    auto stagesGBuffer = mpShaders[GBUFFER_PASS]->getStages();

    cis[GBUFFER_PASS] = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = static_cast<uint32_t>(stagesGBuffer.size()),
        .pStages = stagesGBuffer.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencils[GBUFFER_PASS],
        .pColorBlendState = &colorBlending[GBUFFER_PASS],
        .pDynamicState = &dynamicState,
        .layout = mPipelineLayouts[GBUFFER_PASS],
        .renderPass = mRenderPass,
        .subpass = 0,
    };

    // Create info for resolve pipeline
    auto stagesResolve = mpShaders[RESOLVE_PASS]->getStages();

    VkPipelineVertexInputStateCreateInfo emptyVertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    cis[RESOLVE_PASS] = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = static_cast<uint32_t>(stagesResolve.size()),
        .pStages = stagesResolve.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencils[RESOLVE_PASS],
        .pColorBlendState = &colorBlending[RESOLVE_PASS],
        .pDynamicState = &dynamicState,
        .layout = mPipelineLayouts[RESOLVE_PASS],
        .renderPass = mRenderPass,
        .subpass = 1,
    };

    Check::Vk(
        vkCreateGraphicsPipelines(mpDevice->getDevice(), VK_NULL_HANDLE, 2, cis.data(), nullptr, mPipelines.data()));
}

void Deferred::destroyPipelines()
{
    for (auto pipeline : mPipelines) {
        vkDestroyPipeline(mpDevice->getDevice(), pipeline, nullptr);
    }
}

void Deferred::createRenderPass()
{
    std::array<VkAttachmentDescription, 5> attachments = {};
    // Position
    attachments[0] = {
        .format = mPosition->getFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    // Normal
    attachments[1] = {
        .format = mNormal->getFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    // Albedo
    attachments[2] = {
        .format = mAlbedo->getFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    // Depth
    attachments[3] = {
        .format = mDepth->getFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    // Swapchain
    attachments[4] = {
        .format = mpSwapchain->getImageFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    std::array<VkAttachmentReference, 3> colorAttachmentRefs = {};
    colorAttachmentRefs[0] = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    colorAttachmentRefs[1] = {.attachment = 1, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    colorAttachmentRefs[2] = {.attachment = 2, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentReference swapchainAttachmentRef = {.attachment = 4,
                                                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentReference depthAttachmentRef = {.attachment = 3,
                                                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    std::array<VkAttachmentReference, 3> inputAttachmentRefs = {};
    inputAttachmentRefs[0] = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    inputAttachmentRefs[1] = {.attachment = 1, .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    inputAttachmentRefs[2] = {.attachment = 2, .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    std::array<VkSubpassDescription, 2> subpasses = {};
    subpasses[GBUFFER_PASS] = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size()),
        .pColorAttachments = colorAttachmentRefs.data(),
        .pDepthStencilAttachment = &depthAttachmentRef,
    };
    subpasses[RESOLVE_PASS] = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = static_cast<uint32_t>(inputAttachmentRefs.size()),
        .pInputAttachments = inputAttachmentRefs.data(),
        .colorAttachmentCount = 1,
        .pColorAttachments = &swapchainAttachmentRef,
    };

    std::array<VkSubpassDependency, 3> dependencies = {};
    dependencies[0] = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    };
    dependencies[1] = {
        .srcSubpass = 0,
        .dstSubpass = 1,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    };
    dependencies[2] = {
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    };

    VkRenderPassCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = static_cast<uint32_t>(subpasses.size()),
        .pSubpasses = subpasses.data(),
        .dependencyCount = static_cast<uint32_t>(dependencies.size()),
        .pDependencies = dependencies.data(),
    };

    Check::Vk(vkCreateRenderPass(mpDevice->getDevice(), &ci, nullptr, &mRenderPass));
}

void Deferred::createAttachments()
{
    VkFormat gbufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat albedoFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depthFormat = Helpers::findDepthFormat(mpDevice);
    VkExtent2D extent = mpSwapchain->getExtent();

    mPosition = std::make_unique<Image>(
        mpDevice, extent.width, extent.height, 1, VK_SAMPLE_COUNT_1_BIT, gbufferFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mNormal = std::make_unique<Image>(
        mpDevice, extent.width, extent.height, 1, VK_SAMPLE_COUNT_1_BIT, gbufferFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mAlbedo = std::make_unique<Image>(
        mpDevice, extent.width, extent.height, 1, VK_SAMPLE_COUNT_1_BIT, albedoFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mDepth = std::make_unique<Image>(mpDevice, extent.width, extent.height, 1, VK_SAMPLE_COUNT_1_BIT, depthFormat,
                                     VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Helpers::transitionImageLayout(mpDevice, mDepth->getImage(), depthFormat, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);

    mPosition->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);
    mNormal->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);
    mAlbedo->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);
    mDepth->createImageView(VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Deferred::destroyAttachments()
{
    mPosition = nullptr;
    mNormal = nullptr;
    mAlbedo = nullptr;
    mDepth = nullptr;
}

void Deferred::createFramebuffers()
{
    auto imageViews = mpSwapchain->getImageViews();
    mFramebuffers = std::vector<VkFramebuffer>(imageViews.size());

    for (uint32_t i = 0; i < mFramebuffers.size(); i++) {
        std::array<VkImageView, 5> attachments = {
            mPosition->getImageView(), mNormal->getImageView(), mAlbedo->getImageView(),
            mDepth->getImageView(),    imageViews[i],
        };

        VkFramebufferCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = mRenderPass,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .width = mpSwapchain->getExtent().width,
            .height = mpSwapchain->getExtent().height,
            .layers = 1,
        };

        Check::Vk(vkCreateFramebuffer(mpDevice->getDevice(), &ci, nullptr, &mFramebuffers[i]));
    }
}

void Deferred::destroyFramebuffers()
{
    for (auto& fb : mFramebuffers) {
        vkDestroyFramebuffer(mpDevice->getDevice(), fb, nullptr);
    }
}

void Deferred::frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor)
{
    if (mpSwapchain->recreated()) {
        Log::debug("Recreating framebuffers since swapchain changed");
        destroyFramebuffers();
        destroyAttachments();
        createAttachments();
        createFramebuffers();
    }

    VkCommandBufferBeginInfo bi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    Check::Vk(vkBeginCommandBuffer(cmd, &bi));

    std::array<VkClearValue, 5> clearValues = {};
    clearValues[0].color = {clearColor[0], clearColor[1], clearColor[2], clearColor[3]};
    clearValues[1].color = {clearColor[0], clearColor[1], clearColor[2], clearColor[3]};
    clearValues[2].color = {clearColor[0], clearColor[1], clearColor[2], clearColor[3]};
    clearValues[3].depthStencil = {1.0f, 0};
    clearValues[4].color = {clearColor[0], clearColor[1], clearColor[2], clearColor[3]};

    VkRenderPassBeginInfo rbi = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = mRenderPass,
        .framebuffer = mFramebuffers[mpSwapchain->getImageIndex()],
        .renderArea =
            {
                .offset = {0, 0},
                .extent = mpSwapchain->getExtent(),
            },
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };

    vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[GBUFFER_PASS]);

    // Set dynamic states
    vkCmdSetFrontFace(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(mpSwapchain->getExtent().width),
        .height = static_cast<float>(mpSwapchain->getExtent().height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = mpSwapchain->getExtent(),
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void Deferred::frameEnd(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
    Check::Vk(vkEndCommandBuffer(cmd));
}

void Deferred::nextSubpass(VkCommandBuffer cmd)
{
    vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[RESOLVE_PASS]);
    vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);
}
