#include "Layout.h"

#include "Error.h"

using namespace Mandrill;

Layout::Layout(std::shared_ptr<Device> pDevice, const std::vector<LayoutDescription>& layoutDesc,
               VkDescriptorSetLayoutCreateFlags flags)
    : mpDevice(pDevice)
{
    // Find highest set
    auto maxSet =
        std::max_element(layoutDesc.begin(), layoutDesc.end(), [](auto& a, auto& b) { return a.set < b.set; })[0].set;

    mDescriptorSetLayouts.resize(maxSet + 1);

    // Create bindings for each set
    for (uint32_t set = 0; set < maxSet + 1; set++) {
        std::vector<LayoutDescription> setLayoutDesc;
        std::copy_if(layoutDesc.begin(), layoutDesc.end(), std::back_inserter(setLayoutDesc),
                     [set](const auto& l) { return l.set == set; });

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        // Create each descriptor set layout
        for (auto& l : setLayoutDesc) {
            bindings.push_back({
                .binding = l.binding,
                .descriptorType = l.type,
                .descriptorCount = 1,
                .stageFlags = l.stage,
            });
        }

        VkDescriptorSetLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = flags,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
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
