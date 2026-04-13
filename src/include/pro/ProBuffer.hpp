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
    
    class BufferCopyReceipt {
    private:
        string copyID = "";
        VulkanInitData *refInitData;        // Do NOT clean up!!!
                     
        vk::CommandPool transferCommandPool {};
        vk::CommandBuffer transferCommandBuffer {};
        vk::Fence copyFinished {};   

        vector<vk::BufferMemoryBarrier> allReceiveBarriers {};
        vector<VulkanBuffer> allStageBuffers {};  
                        
    public:
        BufferCopyReceipt(  VulkanInitData &vkInitData, 
                            string copyID,
                            vector<PendingBufferCopy> &allPendingCopies) {   

            // Store init data
            refInitData = &vkInitData;

            // Set copyID
            this->copyID = copyID;

            // Create pool from transfer queue if possible;
            // otherwise, just use graphics queue
            if(refInitData->transferQueue().is_valid) {
                this->transferCommandPool = createVulkanCommandPool(*refInitData, refInitData->transferQueue().index);            
            }
            else {
                this->transferCommandPool = createVulkanCommandPool(*refInitData, refInitData->graphicsQueue().index);            
            }
            
            // Create the fence (but start as UNsignaled)
            this->copyFinished = createVulkanFence(*refInitData, vk::FenceCreateInfo());

            // Create the command buffer
            this->transferCommandBuffer = createVulkanCommandBuffers(*refInitData, transferCommandPool).front();
            
            // Start recording            
            this->transferCommandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

            // For each copy...
            vector<vk::BufferMemoryBarrier> srcOwnershipBarriers {};
            for(auto &pendingCopy : allPendingCopies) {
                // Make the staging buffer
                VulkanBuffer stageBuffer = createStagingBuffer(
                    *refInitData, 
                    pendingCopy.dstBuffer.size,
                    pendingCopy.hostData);
                this->allStageBuffers.push_back(stageBuffer);

                // Record the copy
                vk::BufferCopy copyRegion{};
                copyRegion.size = pendingCopy.dstBuffer.size;
                this->transferCommandBuffer.copyBuffer(stageBuffer.buffer, pendingCopy.dstBuffer.buffer, 1, &copyRegion);
                
                // Are we using the transfer queue?
                if(refInitData->transferQueue().is_valid) {
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
                    this->allReceiveBarriers.push_back(gbarrier);
                }
            }

            // If we have source ownership barriers...
            if(srcOwnershipBarriers.size() > 0) {
                // Do the source ownership barriers at the bottom of the pipeline
                this->transferCommandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eBottomOfPipe, 
                    vk::DependencyFlags(), 
                    0, nullptr, 
                    (uint32_t)srcOwnershipBarriers.size(), srcOwnershipBarriers.data(), 
                    0, nullptr
                );
            }

            // End recording
            this->transferCommandBuffer.end();
        };
        
        ~BufferCopyReceipt() {
            // Cleanup staging buffers
            for(unsigned int i = 0; i < this->allStageBuffers.size(); i++) {
                cleanupVulkanBuffer(*refInitData, this->allStageBuffers[i]);
            }
            this->allStageBuffers.clear();

            // Cleanup receive barriers
            this->allReceiveBarriers.clear();

            // Cleanup fence
            cleanupVulkanFence(*refInitData, this->copyFinished);

            // Free command pool (and command buffer)
            cleanupVulkanCommandPool(*refInitData, this->transferCommandPool);
        };

        void submit() {
            vk::SubmitInfo submitInfo{};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &(transferCommandBuffer);
            
            vk::Queue chosenQueue;
            if(refInitData->transferQueue().is_valid) {
                chosenQueue = refInitData->transferQueue().queue;
            }
            else {
                chosenQueue = refInitData->graphicsQueue().queue;
            }

            chosenQueue.submit(1, &submitInfo, this->copyFinished);
        };

        bool isCopyFinished() {
            vk::Result status = refInitData->device().getFenceStatus(this->copyFinished);
            return (status == vk::Result::eSuccess);
        };

        bool waitForCopyFinished() {
            vk::Result status = refInitData->device().waitForFences(this->copyFinished, true, UINT64_MAX);
            return (status == vk::Result::eSuccess);
        };

        void queueReceiveBarriers(vk::CommandBuffer &graphicsCommandBuffer) {
            // Did we have a transfer queue?
            if(refInitData->transferQueue().is_valid) {
                // Queue up barriers
                graphicsCommandBuffer.pipelineBarrier(                        
                    vk::PipelineStageFlagBits::eTransfer,                        
                    vk::PipelineStageFlagBits::eVertexInput,                        
                    vk::DependencyFlags(),
                    0, nullptr,
                    (uint32_t)this->allReceiveBarriers.size(), 
                    this->allReceiveBarriers.data(),
                    0, nullptr
                );
            }
        };

        string getCopyID() { return copyID; };            
    };


    class TransferManager {
    private:       
        unordered_map<string, BufferCopyReceipt*> copiesInProgress;
        VulkanInitData *refInitData;         // Do NOT clean up!!!

    public:
        TransferManager(VulkanInitData &vkInitData) {
            // Store init data
            refInitData = &vkInitData;
        };

        ~TransferManager() {
            // Cleanup any residual copies
            for (auto it = copiesInProgress.begin(); it != copiesInProgress.end(); ) {
                BufferCopyReceipt *receipt = (it->second);
                delete receipt;
                it++;
            }
            copiesInProgress.clear();     
        };

        BufferCopyReceipt* submitCopies(string copyID, vector<PendingBufferCopy> &allPendingCopies) {
            // Create the buffer receipt
            BufferCopyReceipt *receipt = new BufferCopyReceipt(*refInitData, copyID, allPendingCopies);

            // Submit copies to (transfer?) queue
            receipt->submit();

            // Add to copies in progress            
            copiesInProgress[copyID] = receipt;
            
            // Return our receipt
            return receipt;
        };

        bool checkCompleted(BufferCopyReceipt *receipt, 
                            vk::CommandBuffer &graphicsCommandBuffer) {

            bool isFinished = false;

            if (receipt->isCopyFinished()) {
                // Queue up receive barriers (for ending ownership handshake)
                receipt->queueReceiveBarriers(graphicsCommandBuffer);
                
                // Remove from list of pending copies                
                copiesInProgress.erase(receipt->getCopyID());

                // Cleanup receipt
                delete receipt;

                // Completed!
                isFinished = true;
            }

            return isFinished;
        };

        bool checkAnyCompleted( vector<BufferCopyReceipt*> &allReceipts, 
                                vk::CommandBuffer &graphicsCommandBuffer,
                                vector<string> &receiptsComplete) {
                                
            bool anyComplete = false;


            for(auto it = allReceipts.begin(); it != allReceipts.end(); ) {
                BufferCopyReceipt* receipt = *it;
                string copyID = receipt->getCopyID();
                if(checkCompleted(*it, graphicsCommandBuffer)) {                    
                    it = allReceipts.erase(it);   
                    receiptsComplete.push_back(copyID);
                    anyComplete = true;             
                }
                else {
                    it++;
                }
            }

            return anyComplete;
        };

        void waitUntilCompleted(vector<BufferCopyReceipt*> &allReceipts,                                
                                vector<string> &receiptsComplete) {    

            // Make graphics queue pool, buffer, and fence
            vk::CommandPool blockGraphicsPool = createVulkanCommandPool(*refInitData, refInitData->graphicsQueue().index);
            vk::CommandBuffer blockGraphicsBuffer = createVulkanCommandBuffers(*refInitData, blockGraphicsPool).front();
            vk::Fence blockFence = createVulkanFence(*refInitData, vk::FenceCreateInfo());

            // Start recording            
            blockGraphicsBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

            // For each receipt...
            for(int i = 0; i < allReceipts.size(); i++) {
                string copyID = allReceipts[i]->getCopyID();
                receiptsComplete.push_back(copyID); // All will be done by the end...
                checkCompleted(allReceipts[i], blockGraphicsBuffer);
            }

            // End recording
            blockGraphicsBuffer.end();

            // Submit...
            vk::SubmitInfo submitInfo{};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &(blockGraphicsBuffer);            
            refInitData->graphicsQueue().queue.submit(1, &submitInfo, blockFence);

            // Block until fence complete...
            refInitData->device().waitForFences(blockFence, true, UINT64_MAX);

            // Cleanup command pool and fence
            cleanupVulkanCommandPool(*refInitData, blockGraphicsPool);    
            cleanupVulkanFence(*refInitData, blockFence);
        };
    };
}
