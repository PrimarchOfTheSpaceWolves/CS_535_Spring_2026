#pragma once
#include "ProSetup.hpp"

namespace pro {

    ///////////////////////////////////////////////////////////////////////////
    // FUNCTION POINTERS 
    ///////////////////////////////////////////////////////////////////////////

    using OnResizeFunc = std::function<void()>;
    
    ///////////////////////////////////////////////////////////////////////////
    // STRUCTS 
    ///////////////////////////////////////////////////////////////////////////
    
    struct FrameCommandData {
        vk::CommandPool commandPool {};
        vk::CommandBuffer commandBuffer {};
        vk::Semaphore imageAvailable {};        
        vk::Fence inFlight {};        
    };

    ///////////////////////////////////////////////////////////////////////////
    // FUNCTIONS 
    ///////////////////////////////////////////////////////////////////////////

    inline vk::CommandPool createVulkanCommandPool(
        const VulkanInitData &vkInitData, 
        unsigned int queueIndex,
        vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer) {

        return vkInitData.device().createCommandPool(
            vk::CommandPoolCreateInfo(flags, queueIndex));   
    };

    inline void cleanupVulkanCommandPool(
        const VulkanInitData &vkInitData, 
        vk::CommandPool &commandPool) {

        vkInitData.device().destroyCommandPool(commandPool);
        commandPool = vk::CommandPool();
    };

    inline vector<vk::CommandBuffer> createVulkanCommandBuffers(
        const VulkanInitData &vkInitData, 
        vk::CommandPool &commandPool,
        vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary,
        unsigned int count = 1) {

        return vkInitData.device().allocateCommandBuffers(
            vk::CommandBufferAllocateInfo(commandPool, level, count));            
    };

    inline vk::Fence createVulkanFence(
        VulkanInitData &vkInitData,
        vk::FenceCreateInfo createInfo = vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)) {

        return vkInitData.device().createFence(createInfo);
    };

    inline void cleanupVulkanFence(VulkanInitData &vkInitData, vk::Fence &f) {
        vkInitData.device().destroyFence(f);
        f = vk::Fence();
    }; 

    inline vk::Semaphore createVulkanSemaphore(
        VulkanInitData &vkInitData,
        vk::SemaphoreCreateInfo createInfo = vk::SemaphoreCreateInfo()) {

        return vkInitData.device().createSemaphore(createInfo);
    };

    inline void cleanupVulkanSemaphore(VulkanInitData &vkInitData, vk::Semaphore &s) {
        vkInitData.device().destroySemaphore(s);
        s = vk::Semaphore();
    };

    inline FrameCommandData createFrameCommandData(VulkanInitData &vkInitData) {

        // Initial struct
        FrameCommandData commandData {};

        // Create pool
        commandData.commandPool = createVulkanCommandPool(vkInitData, vkInitData.graphicsQueue().index);

        // Create command buffer
        commandData.commandBuffer = createVulkanCommandBuffers(vkInitData, commandData.commandPool).front();

        // Create semaphore
        commandData.imageAvailable = createVulkanSemaphore(vkInitData);
        
        // Create fence
        commandData.inFlight = createVulkanFence(vkInitData);        
        
        // Return data
        return commandData;
    };

    inline void cleanupFrameCommandData(VulkanInitData &vkInitData, FrameCommandData &commandData) {        
        cleanupVulkanFence(vkInitData, commandData.inFlight);        
        cleanupVulkanSemaphore(vkInitData, commandData.imageAvailable);
        cleanupVulkanCommandPool(vkInitData, commandData.commandPool);
        commandData = {};
    };
     
    inline unsigned int acquireNextSwapImage(   VulkanInitData &vkInitData, 
                                                FrameCommandData &commandData,
                                                OnResizeFunc resizeFunc) {

        // Before a frame-in-flight can start rendering, it needs:
        // - CPU must wait until command buffer has finished all rendering commands (GPU)
        // - Index of swap image to use
        // - Signal (from imageAvail semaphore) that this particular swap image IS actually done presenting

        // CPU waiting until frame-in-flight has completed any rendering commands.
        vkInitData.device().waitForFences(commandData.inFlight, true, UINT64_MAX);

        // Get next swap image.
        // This MAY return before swap image is actually done presenting,
        // hence why we will have our rendering commands wait until imageAvail signals
        unsigned int indexSwap = -1;
        bool successfulAcquire = true;
        do {
            successfulAcquire = true;
            auto frameResult = vkInitData.device().acquireNextImageKHR( vkInitData.swapchain().chain,
                                                                        UINT64_MAX, 
                                                                        commandData.imageAvailable,
                                                                        nullptr);   
            
            // Is this out of date?
            if (frameResult.result == vk::Result::eErrorOutOfDateKHR) {
                resizeFunc();
                successfulAcquire = false;
            }

            // Get swap image index
            indexSwap = frameResult.value;
        } while(!successfulAcquire);

        // Reset the fence since we're about to submit work
        vkInitData.device().resetFences(commandData.inFlight);

        // Return SWAP image index
        return indexSwap;      
    };

    inline void submitToGraphicsQueue(  VulkanInitData &vkInitData, 
                                        FrameCommandData &commandData,
                                        unsigned int indexSwap,
                                        OnResizeFunc resizeFunc) {

        // With our submission of render commands, we want:
        // - To WAIT until the swap image is actually available
        // - When we're done, to SIGNAL the (PER SWAP IMAGE) render finished semaphore

        // Set up our semaphores and stages to wait on
        vk::Semaphore waitSemaphores[] = {commandData.imageAvailable};
        vk::Semaphore signalSemaphores[] = {vkInitData.swapchain().swaps[indexSwap].renderDone};
        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eAllCommands}; //eColorAttachmentOutput};

        // Prepare submission and submit
        // (Noting that we will also signal the fence as well)
        vk::SubmitInfo submitInfo(
            waitSemaphores,
            waitStages,
            commandData.commandBuffer,
            signalSemaphores);
                    
        vkInitData.graphicsQueue().queue.submit(submitInfo, commandData.inFlight);            
    };

    inline bool presentSwapImage(   VulkanInitData &vkInitData, 
                                    FrameCommandData &commandData,
                                    unsigned int indexSwap,
                                    OnResizeFunc resizeFunc) {

        vk::PresentInfoKHR presentInfo {};
        presentInfo.setWaitSemaphores(vkInitData.swapchain().swaps[indexSwap].renderDone);
        presentInfo.setSwapchains(vkInitData.swapchain().chain);
        presentInfo.setImageIndices(indexSwap);
               
        bool successPresent = true;
        try {
            vk::Result presentRes = vkInitData.presentQueue().queue.presentKHR(presentInfo);
        }
        catch (const vk::SystemError& e) {
            if (e.code() == vk::Result::eErrorOutOfDateKHR ||
                e.code() == vk::Result::eSuboptimalKHR) {
                resizeFunc();
                successPresent = false;
            } 
            else {
                throw;  // Throws original exception (not copy)
            }
        }

        return successPresent;
    };
}
