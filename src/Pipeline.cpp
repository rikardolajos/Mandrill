#include "Pipeline.h"

#include "Error.h"
#include "Helpers.h"

using namespace Mandrill;

Pipeline::Pipeline(ptr<Device> pDevice, ptr<Pass> pPass, ptr<Layout> pLayout, ptr<Shader> pShader,
                   const PipelineDesc& desc)
    : mpDevice(pDevice), mpShader(pShader), mpLayout(pLayout), mpPass(pPass), mPipeline(nullptr), mDesc(desc)
{
    const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts = mpLayout->getDescriptorSetLayouts();
    const std::vector<VkPushConstantRange>& pushConstantLayout = mpLayout->getPushConstantRanges();

    VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = count(descriptorSetLayouts),
        .pSetLayouts = descriptorSetLayouts.data(),
        .pushConstantRangeCount = count(pushConstantLayout),
        .pPushConstantRanges = pushConstantLayout.data(),
    };

    Check::Vk(vkCreatePipelineLayout(mpDevice->getDevice(), &ci, nullptr, &mPipelineLayout));

    if (pPass) {
        createPipeline();
    }
}

Pipeline::~Pipeline()
{
    destroyPipeline();
    vkDestroyPipelineLayout(mpDevice->getDevice(), mPipelineLayout, nullptr);
}

void Pipeline::bind(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

    // Set dynamic states
    vkCmdSetFrontFace(cmd, mFrontFace);
    vkCmdSetCullMode(cmd, mCullMode);
    vkCmdSetLineWidth(cmd, mLineWidth);

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(mpPass->getExtent().width),
        .height = static_cast<float>(mpPass->getExtent().height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = mpPass->getExtent(),
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void Pipeline::recreate()
{
    destroyPipeline();
    mpShader->reload();
    createPipeline();
}

void Pipeline::createPipeline()
{
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = count(mDesc.bindingDescriptions),
        .pVertexBindingDescriptions = mDesc.bindingDescriptions.data(),
        .vertexAttributeDescriptionCount = count(mDesc.attributeDescriptions),
        .pVertexAttributeDescriptions = mDesc.attributeDescriptions.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = mDesc.topology,
        .primitiveRestartEnable = mDesc.primitiveRestartEnable,
    };

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_CULL_MODE, VK_DYNAMIC_STATE_FRONT_FACE, VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,   VK_DYNAMIC_STATE_LINE_WIDTH,
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = count(dynamicStates),
        .pDynamicStates = dynamicStates.data(),
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = mDesc.depthClampEnable,
        .rasterizerDiscardEnable = mDesc.rasterizerDiscardEnable,
        .polygonMode = mDesc.polygonMode,
        .frontFace = mFrontFace,
        .depthBiasEnable = mDesc.depthBiasEnable,
        .depthBiasConstantFactor = mDesc.depthBiasConstantFactor,
        .depthBiasClamp = mDesc.depthBiasClamp,
        .depthBiasSlopeFactor = mDesc.depthBiasSlopeFactor,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = mpPass->getSampleCount(),
        .sampleShadingEnable = mDesc.sampleShadingEnable,
        .minSampleShading = mDesc.minSampleShading,
        .pSampleMask = mDesc.pSampleMask,
        .alphaToCoverageEnable = mDesc.alphaToCoverageEnable,
        .alphaToOneEnable = mDesc.alphaToOneEnableEnable,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = mDesc.blendEnable,
        .srcColorBlendFactor = mDesc.srcColorBlendFactor,
        .dstColorBlendFactor = mDesc.dstColorBlendFactor,
        .colorBlendOp = mDesc.colorBlendOp,
        .srcAlphaBlendFactor = mDesc.srcAlphaBlendFactor,
        .dstAlphaBlendFactor = mDesc.dstAlphaBlendFactor,
        .alphaBlendOp = mDesc.alphaBlendOp,
        .colorWriteMask = mDesc.colorWriteMask,
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = mDesc.depthTestEnable,
        .depthWriteEnable = mDesc.depthWriteEnable,
        .depthCompareOp = mDesc.depthCompareOp,
        .depthBoundsTestEnable = mDesc.depthBoundsTestEnable,
        .stencilTestEnable = mDesc.stencilTestEnable,
        .minDepthBounds = mDesc.minDepthBounds,
        .maxDepthBounds = mDesc.maxDepthBounds,
    };

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
    for (uint32_t i = 0; i < count(mpPass->getColorAttachments()); i++) {
        colorBlendAttachmentStates.push_back(colorBlendAttachment);
    }

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = mDesc.logicOpEnable,
        .logicOp = mDesc.logicOp,
        .attachmentCount = count(colorBlendAttachmentStates),
        .pAttachments = colorBlendAttachmentStates.data(),
    };

    auto stages = mpShader->getStages();
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = mpPass->getPipelineRenderingCreateInfo();

    VkGraphicsPipelineCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipelineRenderingCreateInfo,
        .stageCount = count(stages),
        .pStages = stages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = mPipelineLayout,
        .renderPass = nullptr,
    };
    Check::Vk(vkCreateGraphicsPipelines(mpDevice->getDevice(), VK_NULL_HANDLE, 1, &ci, nullptr, &mPipeline));
}

void Pipeline::destroyPipeline()
{
    vkDeviceWaitIdle(mpDevice->getDevice());
    vkDestroyPipeline(mpDevice->getDevice(), mPipeline, nullptr);
}
