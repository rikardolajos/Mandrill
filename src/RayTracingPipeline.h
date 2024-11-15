#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"
#include "Layout.h"
#include "Shader.h"


namespace Mandrill
{
    class RayTracingPipeline
    {
    public:
        MANDRILL_API RayTracingPipeline(ptr<Device> pDevice, ptr<Shader> pShader, ptr<Layout> pLayout);
        MANDRILL_API ~RayTracingPipeline();

        MANDRILL_API void bind(VkCommandBuffer cmd);

        MANDRILL_API void write(VkCommandBuffer cmd, VkImage image);

        MANDRILL_API void present(VkCommandBuffer cmd, VkImage image);

        MANDRILL_API void recreate();


    private:
        void createPipeline();
        void destroyPipeline();

        void createShaderBindingTable();

        ptr<Device> mpDevice;

        ptr<Shader> mpShader;
        ptr<Layout> mpLayout;

        VkPipeline mPipeline;
        VkPipelineLayout mPipelineLayout;

        std::vector<VkRayTracingShaderGroupCreateInfoKHR> mShaderGroups;

        ptr<Buffer> mpShaderBindingTableBuffer;
    };
} // namespace Mandrill
