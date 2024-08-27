#pragma once

#include "Common.h"

#include "RenderPasses/RenderPass.h"

namespace Mandrill
{
    class Rasterizer : public RenderPass
    {
    public:
        MANDRILL_API Rasterizer(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                                std::vector<std::shared_ptr<Layout>> pLayouts,
                                std::vector<std::shared_ptr<Shader>> pShaders);
        MANDRILL_API ~Rasterizer();

        MANDRILL_API void frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor) override;
        MANDRILL_API void frameEnd(VkCommandBuffer cmd) override;

    protected:
        void createPipelines() override;
        void destroyPipelines() override;

    private:
        void createRenderPass();
    };
} // namespace Mandrill