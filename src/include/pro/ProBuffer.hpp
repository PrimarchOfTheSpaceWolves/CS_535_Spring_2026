#pragma once
#include "ProCommand.hpp"

namespace pro {
    ///////////////////////////////////////////////////////////////////////////
    // STRUCTS 
    ///////////////////////////////////////////////////////////////////////////

    struct VulkanBuffer {
        vk::Buffer buffer = nullptr;
        VmaAllocation allocation{};
        vk::DeviceSize size{};
        vk::BufferUsageFlags usage{};
        void* mapped = nullptr;             
    };

    struct PendingBufferCopy {        
        void *hostData = nullptr;        
        VulkanBuffer dstBuffer {};
        vk::AccessFlags dstAccessMask {};

        PendingBufferCopy(VulkanBuffer &dstBuffer, void *hostData, vk::AccessFlags dstAccessMask) {            
            this->dstBuffer = dstBuffer;
            this->hostData = hostData;
            this->dstAccessMask = dstAccessMask;
        };
    };

    struct BufferCopyReceipt {
        string copyID = "";
        vk::Fence copyFinished {};
        vector<vk::BufferMemoryBarrier> allReceiveBarriers {};
        vector<VulkanBuffer> allStageBuffers {};
        vk::CommandBuffer commandBuffer {};
    };

    ///////////////////////////////////////////////////////////////////////////
    // COMMON DEFAULTS (HELPER FUNCTIONS) 
    ///////////////////////////////////////////////////////////////////////////

    inline VmaAllocationCreateInfo createVMAHostVisibleInfo() {
        VmaAllocationCreateInfo vci{};
        vci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | // Written sequentially (like with memcopy)
                        VMA_ALLOCATION_CREATE_MAPPED_BIT;   // Persistently mapped
        vci.usage = VMA_MEMORY_USAGE_AUTO;
        return vci;
    };

    inline VmaAllocationCreateInfo createVMADeviceLocalInfo() {
        VmaAllocationCreateInfo vci{};
        vci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        return vci;
    };

    ///////////////////////////////////////////////////////////////////////////
    // FUNCTIONS 
    ///////////////////////////////////////////////////////////////////////////  

    inline VulkanBuffer createVulkanBuffer( VulkanInitData &vkInitData,
                                            vk::DeviceSize size,
                                            vk::BufferUsageFlags usage,                                    
                                            VmaAllocationCreateInfo vmaInfo,
                                            vk::SharingMode sharingMode = vk::SharingMode::eExclusive) {
        
        VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size  = size;
        bci.usage = static_cast<VkBufferUsageFlags>(usage);
        bci.sharingMode = static_cast<VkSharingMode>(sharingMode);
        
        VkBuffer rawBuf{};
        VmaAllocation alloc{};
        VmaAllocationInfo ainfo{};
        vmaCreateBuffer(vkInitData.allocator(), &bci, &vmaInfo, &rawBuf, &alloc, &ainfo);

        VulkanBuffer out;
        out.size = size;
        out.usage = usage;
        out.buffer = vk::Buffer(rawBuf);
        out.allocation = alloc;
        out.mapped = ainfo.pMappedData; 
        
        return out;
    };

    inline void cleanupVulkanBuffer(VulkanInitData &vkInitData, VulkanBuffer &bufferData) {
        if(bufferData.buffer) {
            vmaDestroyBuffer(vkInitData.allocator(), static_cast<VkBuffer>(bufferData.buffer), bufferData.allocation);
            bufferData = {};
        }           
    };

    inline void copyToHostVisibleVulkanBuffer(  VulkanInitData &vkInitData,
                                                VulkanBuffer &bufferData,
                                                void *hostData) {

        memcpy(bufferData.mapped, hostData, bufferData.size);
        vmaFlushAllocation(vkInitData.allocator(), bufferData.allocation, 0, VK_WHOLE_SIZE);
    };

    inline VulkanBuffer createStagingBuffer(    VulkanInitData &vkInitData, 
                                                vk::DeviceSize bufferSize, 
                                                void *hostData = nullptr) {

        // Create host-visible staging buffer (TRANSFER_SRC)
        VulkanBuffer stageBuffer = createVulkanBuffer(  vkInitData, 
                                                        bufferSize,
                                                        vk::BufferUsageFlagBits::eTransferSrc,
                                                        createVMAHostVisibleInfo());

        // Copy host data into staging buffer if available
        if(hostData) {
            copyToHostVisibleVulkanBuffer(vkInitData, stageBuffer, hostData);
        }

        // Return staging buffer
        return stageBuffer;
    };

    ///////////////////////////////////////////////////////////////////////////
    // CLASSES 
    ///////////////////////////////////////////////////////////////////////////  

    class TransferManager {
    private:
        vk::CommandPool transferPool {};      
        unordered_map<string, BufferCopyReceipt*> copiesInProgress;
        VulkanInitData *refInitData;         // Do NOT clean up!!!

        void cleanupBufferReceipt(BufferCopyReceipt &receipt) {
            // Cleanup staging buffers
            for(unsigned int i = 0; i < receipt.allStageBuffers.size(); i++) {
                cleanupVulkanBuffer(*refInitData, receipt.allStageBuffers[i]);
            }
            receipt.allStageBuffers.clear();

            // Cleanup receive barriers
            receipt.allReceiveBarriers.clear();

            // Cleanup fence
            cleanupVulkanFence(*refInitData, receipt.copyFinished);

            // Free command buffer
            refInitData->device().freeCommandBuffers(transferPool, 1, &receipt.commandBuffer);

            // Remove from list of pending copies
            copiesInProgress.erase(receipt.copyID);
        }
                
    public:
        TransferManager(VulkanInitData &vkInitData) {
            // Store init data
            refInitData = &vkInitData;

            // Create pool for transfer queue
            transferPool = createVulkanCommandPool(*refInitData, refInitData->transferQueue().index);            
        };

        ~TransferManager() {
            // Cleanup any residual copies
            for (auto it = copiesInProgress.begin(); it != copiesInProgress.end(); ) {
                cleanupBufferReceipt(*(it->second));
                it++;
            }
            copiesInProgress.clear();

            // Cleanup command pool            
            cleanupVulkanCommandPool(*refInitData, transferPool);
        };

        BufferCopyReceipt submitCopies(string copyID, vector<PendingBufferCopy> &allPendingCopies) {
            // Create the struct to hold the receipt
            BufferCopyReceipt receipt {};

            // Create the fence (but start as UNsignaled)
            receipt.copyFinished = createVulkanFence(*refInitData, vk::FenceCreateInfo());

            // Create the command buffer
            receipt.commandBuffer = createVulkanCommandBuffers(*refInitData, transferPool).front();

            // Start recording            
            receipt.commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

            // For each copy...
            vector<vk::BufferMemoryBarrier> srcOwnershipBarriers {};
            for(auto &pendingCopy : allPendingCopies) {
                // Make the staging buffer
                VulkanBuffer stageBuffer = createStagingBuffer(
                    *refInitData, 
                    pendingCopy.dstBuffer.size,
                    pendingCopy.hostData);
                receipt.allStageBuffers.push_back(stageBuffer);

                // Record the copy
                vk::BufferCopy copyRegion{};
                copyRegion.size = pendingCopy.dstBuffer.size;
                receipt.commandBuffer.copyBuffer(stageBuffer.buffer, pendingCopy.dstBuffer.buffer, 1, &copyRegion);
                
                // Create the source ownership transfer barrier
                vk::BufferMemoryBarrier tbarrier{};
                tbarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                tbarrier.dstAccessMask = pendingCopy.dstAccessMask;
                tbarrier.srcQueueFamilyIndex = refInitData->transferQueue().index;
                tbarrier.dstQueueFamilyIndex = refInitData->graphicsQueue().index; 
                tbarrier.buffer = pendingCopy.dstBuffer.buffer;
                tbarrier.size = VK_WHOLE_SIZE;
                srcOwnershipBarriers.push_back(tbarrier);

                // Create the destination ownership transfer barrier
                vk::BufferMemoryBarrier gbarrier{};
                gbarrier.srcAccessMask = vk::AccessFlagBits::eNone;         
                gbarrier.dstAccessMask = pendingCopy.dstAccessMask;
                gbarrier.srcQueueFamilyIndex = refInitData->transferQueue().index;
                gbarrier.dstQueueFamilyIndex = refInitData->graphicsQueue().index; 
                gbarrier.buffer = pendingCopy.dstBuffer.buffer;
                gbarrier.size = VK_WHOLE_SIZE;
                receipt.allReceiveBarriers.push_back(gbarrier);
            }

            // Do the source ownership barriers at the bottom of the pipeline
            receipt.commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eBottomOfPipe, 
                vk::DependencyFlags(), 
                0, nullptr, 
                (uint32_t)srcOwnershipBarriers.size(), srcOwnershipBarriers.data(), 
                0, nullptr
            );

            // End recording
            receipt.commandBuffer.end();

            // Submit
            vk::SubmitInfo submitInfo{};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &receipt.commandBuffer;
            refInitData->transferQueue().queue.submit(1, &submitInfo, receipt.copyFinished);

            // Add to copies in progress
            receipt.copyID = copyID;
            copiesInProgress[copyID] = &receipt;
            
            // Return our receipt
            return receipt;
        };

        bool checkCompleted(BufferCopyReceipt &receipt, vk::CommandBuffer &graphicsCommandBuffer) {
            bool isFinished = false;

            // Have we finished copying?            
            vk::Result status = refInitData->device().getFenceStatus(receipt.copyFinished);

            if (status == vk::Result::eSuccess) {
                // Queue up barriers
                graphicsCommandBuffer.pipelineBarrier(                        
                    vk::PipelineStageFlagBits::eTransfer,                        
                    vk::PipelineStageFlagBits::eVertexInput,                        
                    vk::DependencyFlags(),
                    0, nullptr,
                    (uint32_t)receipt.allReceiveBarriers.size(), 
                    receipt.allReceiveBarriers.data(),
                    0, nullptr
                );

                // Cleanup receipt
                cleanupBufferReceipt(receipt);

                // Completed!
                isFinished = true;
            }

            return isFinished;
        };
    };
}
