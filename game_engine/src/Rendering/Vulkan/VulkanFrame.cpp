#include "Rendering/Vulkan/VulkanFrame.h"

#include "Rendering/IRenderer.h"
#include "Rendering/RenderBackend.h"
#include "Scene/Scene.h"
#include "Rendering/Vulkan/VulkanRenderer.h"
#include "Rendering/RenderTypes.h"

#include <iostream>
#include <stdexcept>

namespace GameEngine {

void VulkanFrame::create(VulkanDevice& device, VulkanResources& resources, VulkanSwapChain& swapChain,
    VulkanPipeline& pipeline)
{
    device_ = &device;
    resources_ = &resources;
    swapChain_ = &swapChain;
    pipeline_ = &pipeline;

    createSyncObjects();
    createSwapchainSyncObjects();
}

void VulkanFrame::createSwapchainSyncObjects() {
    if (!device_ || !swapChain_) {
        return;
    }

    device_->getDevice().waitIdle();

    const size_t imageCount = swapChain_->getSwapChain().getImages().size();
    if (imageCount == 0) {
        throw std::runtime_error("VulkanFrame: swapchain has no images");
    }

    imagesInFlight_.assign(imageCount, vk::Fence{});
    swapchainImagePresented_.assign(imageCount, false);

    // One render-finished semaphore per swapchain image (indexed by acquired imageIndex).
    presentWaitSemaphores_.clear();
    presentWaitSemaphores_.reserve(imageCount);
    renderFinishedSemaphores_.clear();
    renderFinishedSemaphores_.reserve(imageCount);

    vk::SemaphoreCreateInfo semInfo{};
    for (size_t i = 0; i < imageCount; ++i) {
        renderFinishedSemaphores_.emplace_back(device_->getDevice(), semInfo);
        presentWaitSemaphores_.push_back(static_cast<VkSemaphore>(*renderFinishedSemaphores_.back()));
    }
}

void VulkanFrame::renderScene() {
    if (!scene_ || !renderer_) {
        return;
    }

    scene_->render(*renderer_);
}

void VulkanFrame::drawFrame() {
    if (!renderer_) {
        return;
    }
    if (presentWaitSemaphores_.empty()) {
        throw std::runtime_error("VulkanFrame: present wait semaphores not initialized");
    }
    vk::Fence frameFence = *inFlightFences_[frameIndex_];

    // Wait for previous frame to finish on CPU
    const vk::Result frameFenceWaitResult = device_->getDevice().waitForFences(
        {frameFence},
        VK_TRUE,
        UINT64_MAX);
    (void)frameFenceWaitResult;

    uint32_t imageIndex = 0;
    vk::Result result = vk::Result::eSuccess;

    try {
        // Acquire next image
        auto [acquireResult, acquiredImageIndex] =
            swapChain_->getSwapChain().acquireNextImage(
                UINT64_MAX,
                imageAvailableSemaphores_[frameIndex_],
                VK_NULL_HANDLE);
        result = acquireResult;
        imageIndex = acquiredImageIndex;
    } catch (const vk::OutOfDateKHRError&) {
        device_->getDevice().waitIdle();
        swapChain_->recreateSwapChain(*resources_);
        resources_->createUniformBuffers(sizeof(PerFrameUniforms));
        pipeline_->recreateDescriptorSets();
        createSwapchainSyncObjects();
        frameIndex_ = 0;
        return;
    }

    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR)
    {
        device_->getDevice().waitIdle();
        swapChain_->recreateSwapChain(*resources_);
        resources_->createUniformBuffers(sizeof(PerFrameUniforms));
        pipeline_->recreateDescriptorSets();
        createSwapchainSyncObjects();
        frameIndex_ = 0;
        return;
    }

    if (imageIndex >= presentWaitSemaphores_.size()) {
        throw std::runtime_error(
            "VulkanFrame: acquired image index " + std::to_string(imageIndex) +
            " but only " + std::to_string(presentWaitSemaphores_.size()) +
            " present wait semaphores exist");
    }

    if (imagesInFlight_[imageIndex] != vk::Fence{}) {
        const vk::Result imageFenceWaitResult = device_->getDevice().waitForFences({imagesInFlight_[imageIndex]}, VK_TRUE, UINT64_MAX);
        (void)imageFenceWaitResult;
    }
    imagesInFlight_[imageIndex] = frameFence;
    device_->getDevice().resetFences({frameFence});

    if (vulkanRenderer_) {
        vulkanRenderer_->setCurrentImageIndex(imageIndex);
    }
    renderer_->beginFrame();
    renderScene();
    renderer_->endFrame();

    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = *resources_->getCommandPool();
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (commandBuffers_.empty()) {
        commandBuffers_ = device_->getDevice().allocateCommandBuffers(allocInfo);
    }

    auto& cmdBuf = commandBuffers_[frameIndex_];
    cmdBuf.reset();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmdBuf.begin(beginInfo);

    vk::ImageMemoryBarrier swapchainToColor{};
    swapchainToColor.oldLayout = swapchainImagePresented_[imageIndex] ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eUndefined;
    swapchainToColor.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    swapchainToColor.srcAccessMask = {};
    swapchainToColor.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    swapchainToColor.image = swapChain_->getImages().at(imageIndex);
    swapchainToColor.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                           vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                           {}, {}, {}, swapchainToColor);

    // Begin dynamic rendering
    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView = *swapChain_->getImageViews().at(imageIndex);
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    vk::ClearValue clearColor = vk::ClearValue(vk::ClearColorValue(std::array<float,4>{0.2f,0.3f,0.3f,1.0f}));
    colorAttachment.clearValue = clearColor;

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.imageView = *resources_->getDepthImageView();
    depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    vk::ClearValue clearDepth = vk::ClearValue(vk::ClearDepthStencilValue{1.0f, 0});
    depthAttachment.clearValue = clearDepth;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea.offset = vk::Offset2D{0, 0};
    renderingInfo.renderArea.extent = swapChain_->getExtent();
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    cmdBuf.beginRendering(renderingInfo);

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChain_->getExtent().width);
    viewport.height = static_cast<float>(swapChain_->getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    std::array<vk::Viewport, 1> viewports = { viewport };
    cmdBuf.setViewport(0, viewports);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = swapChain_->getExtent();
    std::array<vk::Rect2D, 1> scissors = { scissor };
    cmdBuf.setScissor(0, scissors);

    cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_->getGraphicsPipeline());
    cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_->getGraphicsPipelineLayout(), 0, {pipeline_->getDescriptorSet(imageIndex)}, {});

    if (vulkanRenderer_) {
        vulkanRenderer_->recordRenderCommands(static_cast<vk::CommandBuffer>(*cmdBuf), imageIndex);
    }

    cmdBuf.endRendering();

    vk::ImageMemoryBarrier colorToPresent{};
    colorToPresent.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorToPresent.newLayout = vk::ImageLayout::ePresentSrcKHR;
    colorToPresent.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    colorToPresent.dstAccessMask = {};
    colorToPresent.image = swapChain_->getImages().at(imageIndex);
    colorToPresent.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                           vk::PipelineStageFlagBits::eBottomOfPipe,
                           {}, {}, {}, colorToPresent);

    cmdBuf.end();

    vk::SubmitInfo submitInfo{};
    vk::Semaphore waitSemaphores[] = { *imageAvailableSemaphores_[frameIndex_] };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    vk::CommandBuffer cmd = *cmdBuf;
    submitInfo.pCommandBuffers = &cmd;
    VkSemaphore renderFinishedSemaphore = presentWaitSemaphores_[imageIndex];
    vk::Semaphore signalSemaphores[] = { renderFinishedSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    device_->getQueue().submit(submitInfo, *inFlightFences_[frameIndex_]);

    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    vk::SwapchainKHR sc = swapChain_->getSwapChain();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &sc;
    presentInfo.pImageIndices = &imageIndex;

    vk::Result presentResult = vk::Result::eSuccess;
    try {
        presentResult = static_cast<vk::Result>(device_->getQueue().presentKHR(presentInfo));
    } catch (const vk::OutOfDateKHRError&) {
        device_->getDevice().waitIdle();
        swapChain_->recreateSwapChain(*resources_);
        resources_->createUniformBuffers(sizeof(PerFrameUniforms));
        pipeline_->recreateDescriptorSets();
        createSwapchainSyncObjects();
        frameIndex_ = 0;
        return;
    }
    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
        device_->getDevice().waitIdle();
        swapChain_->recreateSwapChain(*resources_);
        resources_->createUniformBuffers(sizeof(PerFrameUniforms));
        pipeline_->recreateDescriptorSets();
        createSwapchainSyncObjects();
        frameIndex_ = 0;
        return;
    }

    swapchainImagePresented_[imageIndex] = true;

    frameIndex_ = (frameIndex_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanFrame::createSyncObjects() {
    if (!device_) {
        return;
    }

    // Per-frame-in-flight: acquire semaphores and submission fences only.
    // renderFinishedSemaphores_ are owned by createSwapchainSyncObjects (one per swapchain image).
    imageAvailableSemaphores_.clear();
    imageAvailableSemaphores_.reserve(MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semInfo{};
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imageAvailableSemaphores_.emplace_back(device_->getDevice(), semInfo);
    }

    timelineValue = 0;
    inFlightFences_.clear();
    inFlightFences_.reserve(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vk::FenceCreateInfo fenceInfo{};
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
        inFlightFences_.emplace_back(device_->getDevice(), fenceInfo);
    }
}

}