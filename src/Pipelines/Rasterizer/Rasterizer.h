#pragma once

#include "Common.h"

#include "Pipelines/Pipeline.h"

namespace Mandrill
{
    class Rasterizer : public Pipeline
    {
    public:
        MANDRILL_API Rasterizer(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                                std::vector<LayoutCreator>& layout, std::shared_ptr<Shader> pShader);
        MANDRILL_API ~Rasterizer();

        MANDRILL_API void frameBegin(VkCommandBuffer cmd, float clearColor[4]) override;
        MANDRILL_API void frameEnd(VkCommandBuffer cmd) override;

    protected:
        void createPipeline() override;
        void destroyPipeline() override;
        void recreatePipeline() override;

    private:
        void createRenderPass();
    };
} // namespace Mandrill