#pragma once
#include "ProSetup.hpp"

namespace pro {

    ///////////////////////////////////////////////////////////////////////////
    // STRUCTS 
    ///////////////////////////////////////////////////////////////////////////
    
    struct VulkanImageTransition {
        vk::ImageMemoryBarrier barrier {};
        vk::PipelineStageFlags srcFlags {};
        vk::PipelineStageFlags dstFlags {};
    };

    enum IMAGE_TRANSITION_TYPE {
        UNDEF_TO_COLOR,        
        COLOR_TO_PRESENT,
        UNDEF_TO_DEPTH   
    };

    struct VulkanImage {
        vk::Image image{};
        vk::ImageView view{};
        VmaAllocation allocation{};
        vk::Format format{};
        vk::Extent3D extent{};
        uint32_t mipLevels{1};
    };

    ///////////////////////////////////////////////////////////////////////////
    // FUNCTIONS 
    ///////////////////////////////////////////////////////////////////////////

    inline VulkanImageTransition createVulkanImageTransition(
        const vk::Image &image, 
        IMAGE_TRANSITION_TYPE type) {

        VulkanImageTransition transitionData {};

        vk::ImageLayout oldLayout {};
        vk::ImageLayout newLayout {};
        vk::AccessFlags srcMask {};
        vk::AccessFlags dstMask {};
        vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor;

        switch(type) {
            case UNDEF_TO_COLOR:
            {
                oldLayout = vk::ImageLayout::eUndefined;
                newLayout = vk::ImageLayout::eColorAttachmentOptimal;
                dstMask = vk::AccessFlagBits::eColorAttachmentWrite;

                transitionData.srcFlags = vk::PipelineStageFlagBits::eTopOfPipe;
                transitionData.dstFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                break;
            }            
            case COLOR_TO_PRESENT:
            {
                oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
                newLayout = vk::ImageLayout::ePresentSrcKHR;
                srcMask = vk::AccessFlagBits::eColorAttachmentWrite;   
                
                transitionData.srcFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                transitionData.dstFlags = vk::PipelineStageFlagBits::eBottomOfPipe;
                break;
            }
            case UNDEF_TO_DEPTH:
            {
                oldLayout = vk::ImageLayout::eUndefined;
                newLayout = vk::ImageLayout::eDepthAttachmentOptimal;
                dstMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite
                             | vk::AccessFlagBits::eDepthStencilAttachmentRead;

                transitionData.srcFlags = vk::PipelineStageFlagBits::eTopOfPipe;
                transitionData.dstFlags = vk::PipelineStageFlagBits::eEarlyFragmentTests
                                         | vk::PipelineStageFlagBits::eLateFragmentTests;
                
                aspectFlags = vk::ImageAspectFlagBits::eDepth;
                break;
            }            
            default:
            {
                throw invalid_argument("Unsupported layout transition!");
                break;
            }
        }

        // Create the actual memory barrier
        vk::ImageMemoryBarrier barrier {};
        barrier.setOldLayout(oldLayout);
        barrier.setNewLayout(newLayout);
        barrier.setSrcAccessMask(srcMask);
        barrier.setDstAccessMask(dstMask);
        barrier.setImage(image);
        barrier.setSubresourceRange(
            vk::ImageSubresourceRange(aspectFlags, 0, 1, 0, 1));

        transitionData.barrier = barrier;

        // Return data
        return transitionData;
    }

    inline void performVulkanImageTransition(
        vk::CommandBuffer &commandBuffer, 
        VulkanImageTransition &transitionData) {

        commandBuffer.pipelineBarrier(
            transitionData.srcFlags,
            transitionData.dstFlags,
            {}, nullptr, nullptr,
            transitionData.barrier);
    };

    inline void performVulkanImageTransition(
        vk::CommandBuffer &commandBuffer, 
        const vk::Image &image, 
        IMAGE_TRANSITION_TYPE type) {

        VulkanImageTransition transition = createVulkanImageTransition(image, type);
        performVulkanImageTransition(commandBuffer, transition);
    };

    inline VulkanImage createVulkanImage(  
                                    const VulkanInitData &vkInitData,
                                    vk::Extent3D extent,
                                    vk::Format format, vk::ImageUsageFlags usage,
                                    vk::ImageAspectFlags aspectFlags,
                                    uint32_t mipLevels,
                                    vk::SampleCountFlagBits samples) {

        // Create struct
        VulkanImage imageData{};
        imageData.extent = extent;
        imageData.format = format;
        imageData.mipLevels = mipLevels;
        
        // Set up creation info
        vk::ImageCreateInfo imgInfo {};
        imgInfo.imageType = vk::ImageType::e2D;
        imgInfo.extent = extent;
        imgInfo.mipLevels = mipLevels;        
        imgInfo.samples = samples;
        imgInfo.format = format;
        imgInfo.usage = usage;
        imgInfo.initialLayout = vk::ImageLayout::eUndefined;// Start undefined 
                                                            // (will have to transition later)
        imgInfo.arrayLayers = 1;                            // Not an array
        imgInfo.tiling = vk::ImageTiling::eOptimal;         // Layout memory efficiently 
                                                            // (can't read texels easily ourselves)
        imgInfo.sharingMode = vk::SharingMode::eExclusive;  // Only used by one queue family

        // Want data on device
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        // Actually allocate data
        VkImageCreateInfo vkImgInfo = static_cast<VkImageCreateInfo>(imgInfo);
        VkImage image;
        VmaAllocation alloc;
        vmaCreateImage(vkInitData.allocator(), &vkImgInfo, &allocInfo, 
                        &image, &alloc, nullptr);

        // Save image and allocation
        imageData.image = vk::Image { image };
        imageData.allocation = alloc;

        // Also create image view while we're here
        vk::ImageViewCreateInfo viewInfo {};
        viewInfo.image = imageData.image;
        viewInfo.format = format;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.subresourceRange = { aspectFlags, 0, 1, 0, 1 }; 
        // Aspect that are visible (also mipmap level and array ranges)
   
        imageData.view = vkInitData.device().createImageView(viewInfo);

        // Return data
        return imageData;
    };

    inline void cleanupVulkanImage(
        const VulkanInitData &vkInitData, 
        VulkanImage &imageData) {

        vkInitData.device().destroyImageView(imageData.view);
        vmaDestroyImage(vkInitData.allocator(), imageData.image, imageData.allocation);
        imageData = {};
    };

    inline vk::RenderingAttachmentInfoKHR createColorAttachment(
        const vk::ImageView &swapImageView,
        vk::ClearColorValue clearColor) {

        vk::RenderingAttachmentInfoKHR colorAtt {};
        colorAtt.setImageView(swapImageView)
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(clearColor);
        return colorAtt;
    };

    inline vk::RenderingAttachmentInfoKHR createDepthAttachment(const vk::ImageView &depthImageView) {
        vk::RenderingAttachmentInfoKHR depthAtt {};
        depthAtt.setImageView(depthImageView)
                .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setClearValue(vk::ClearDepthStencilValue {1.0f, 0});
        return depthAtt;
    };
    
    inline void cleanupAllVulkanDepthImages(
        const VulkanInitData &vkInitData,
        vector<VulkanImage> &allDepthImages) {
        for(int i = 0; i < allDepthImages.size(); i++) {
            cleanupVulkanImage(vkInitData, allDepthImages.at(i));
        }
        allDepthImages.clear();
    };
    
    inline void recreateAllVulkanDepthImages(
        const VulkanInitData &vkInitData,
        vector<VulkanImage> &allDepthImages,
        int numberFramesInFlight) {

        // Make sure device is idle
        vkInitData.device().waitIdle();

        // Create a temporary command pool and buffer for image transitions
        vk::CommandPool depthCommandPool = vkInitData.device().createCommandPool(
                                vk::CommandPoolCreateInfo(
                                    vk::CommandPoolCreateFlags(
                                        vk::CommandPoolCreateFlagBits::eTransient |
                                        vk::CommandPoolCreateFlagBits::eResetCommandBuffer),
                                        vkInitData.graphicsQueue().index));   
        vk::CommandBuffer depthCommandBuffer = vkInitData.device().allocateCommandBuffers(
                                        vk::CommandBufferAllocateInfo(
                                            depthCommandPool, 
                                            vk::CommandBufferLevel::ePrimary, 
                                            1)
                                        ).front(); 

        // Start recording...       
        depthCommandBuffer.reset();     
        depthCommandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    
        // Cleanup previous images if there are any...
        if(allDepthImages.size() > 0) {            
            cleanupAllVulkanDepthImages(vkInitData, allDepthImages);
        }

        // For each swap image...
        for(unsigned int i = 0; i < numberFramesInFlight; i++) {
            VulkanImage depthImage =  createVulkanImage(   
                                        vkInitData, 
                                        vk::Extent3D { 
                                            vkInitData.swapchain().extent.width, 
                                            vkInitData.swapchain().extent.height, 
                                            1 },
                                        vk::Format::eD32Sfloat,
                                        vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                        vk::ImageAspectFlagBits::eDepth,
                                        1, vk::SampleCountFlagBits::e1);          
            allDepthImages.push_back(depthImage);           
            performVulkanImageTransition(depthCommandBuffer, depthImage.image, IMAGE_TRANSITION_TYPE::UNDEF_TO_DEPTH);
        }

        // End recording
        depthCommandBuffer.end();
        // Submit to queue
        vk::SubmitInfo submitInfo = vk::SubmitInfo().setCommandBuffers(depthCommandBuffer);                    
        vkInitData.graphicsQueue().queue.submit(submitInfo);
        vkInitData.graphicsQueue().queue.waitIdle();

        // Cleanup command pool
        vkInitData.device().destroyCommandPool(depthCommandPool);
    }; 
}