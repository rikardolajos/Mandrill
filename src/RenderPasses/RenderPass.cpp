#include "RenderPass.h"

#include "Error.h"
#include "Log.h"

using namespace Mandrill;

RenderPass::RenderPass(ptr<Device> pDevice, ptr<Swapchain> pSwapchain)
    : mpDevice(pDevice), mpSwapchain(pSwapchain), mRenderPass(VK_NULL_HANDLE)
{
}

RenderPass::~RenderPass()
{
}

void RenderPass::recreate()
{
    Log::debug("Recreating render pass");
    destroyFramebuffers();
    destroyAttachments();
    createAttachments();
    createFramebuffers();
}
