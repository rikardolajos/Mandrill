#include "Layout.h"

#include "Error.h"

using namespace Mandrill;

Layout::Layout(ptr<Device> pDevice, const std::vector<LayoutDesc>& desc, VkDescriptorSetLayoutCreateFlags flags)
    : mpDevice(pDevice)
{
    if (desc.empty()) {
        return;
    }

    // Find highest set
    auto maxSet = std::max_element(desc.begin(), desc.end(), [](auto& a, auto& b) { return a.set < b.set; })[0].set;

    mDescriptorSetLayouts.resize(maxSet + 1);

    // Create bindings for each set
    for (uint32_t set = 0; set < maxSet + 1; set++) {
        std::vector<LayoutDesc> setLayoutDesc;
        std::copy_if(desc.begin(), desc.end(), std::back_inserter(setLayoutDesc),
                     [set](const auto& l) { return l.set == set; });

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        // Create each descriptor set layout
        for (auto& l : setLayoutDesc) {
            bindings.push_back({
                .binding = l.binding,
                .descriptorType = l.type,
                .descriptorCount = l.arrayCount ? l.arrayCount : 1,
                .stageFlags = l.stage,
            });
        }

        VkDescriptorSetLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = flags,
            .bindingCount = count(bindings),
            .pBindings = bindings.data(),
        };

        Check::Vk(vkCreateDescriptorSetLayout(mpDevice->getDevice(), &ci, nullptr, &mDescriptorSetLayouts[set]));
    }
}

Layout::~Layout()
{
    for (auto& l : mDescriptorSetLayouts) {
        vkDestroyDescriptorSetLayout(mpDevice->getDevice(), l, nullptr);
    }
}
