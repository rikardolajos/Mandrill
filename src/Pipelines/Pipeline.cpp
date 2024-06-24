#include "Pipeline.h"

#include "Error.h"

using namespace Mandrill;

Pipeline::Pipeline(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                   std::vector<LayoutCreator>& layout, std::shared_ptr<Shader> pShader)
    : mpDevice(pDevice), mpSwapchain(pSwapchain), mpShader(pShader)
{
    createLayout(layout);
}

Pipeline::~Pipeline()
{
    for (auto& l : mDescriptorSetLayouts) {
        vkDestroyDescriptorSetLayout(mpDevice->getDevice(), l, nullptr);
    }
    vkDestroyPipelineLayout(mpDevice->getDevice(), mLayout, nullptr);
}

void Pipeline::createLayout(std::vector<LayoutCreator>& layout)
{
    // Find highest set
    auto maxSet = std::max_element(layout.begin(), layout.end(), [](auto& a, auto& b) { return a.set < b.set; })[0].set;

    mDescriptorSetLayouts.resize(maxSet + 1);

    // Create bindings for each set
    for (uint32_t set = 0; set < maxSet + 1; set++) {
        std::vector<LayoutCreator> setLayout;
        std::copy_if(layout.begin(), layout.end(), std::back_inserter(setLayout),
                     [set](const auto& l) { return l.set == set; });

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        // Create each descriptor set layout
        for (auto& l : setLayout) {
            bindings.push_back({
                .binding = l.binding,
                .descriptorType = l.type,
                .descriptorCount = 1,
                .stageFlags = l.stage,
            });
        }

        VkDescriptorSetLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        };

        Check::Vk(vkCreateDescriptorSetLayout(mpDevice->getDevice(), &ci, nullptr, &mDescriptorSetLayouts[set]));
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo lci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(mDescriptorSetLayouts.size()),
        .pSetLayouts = mDescriptorSetLayouts.data(),
    };

    Check::Vk(vkCreatePipelineLayout(mpDevice->getDevice(), &lci, nullptr, &mLayout));
}
