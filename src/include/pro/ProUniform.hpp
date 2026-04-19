#pragma once
#include "ProBuffer.hpp"

namespace pro {

    ///////////////////////////////////////////////////////////////////////////
    // STRUCT
    ///////////////////////////////////////////////////////////////////////////

    struct DescriptorPack {
        vk::DescriptorPool pool {};
        vector<vk::DescriptorSet> sets {};
    };
    
    ///////////////////////////////////////////////////////////////////////////
    // FUNCTIONS
    ///////////////////////////////////////////////////////////////////////////

    inline DescriptorPack createDescriptorPack( VulkanInitData &vkInitData,
                                                VulkanPipelineData &pipelineData,
                                                vector<vk::DescriptorPoolSize> &poolSizes,
                                                int maxSets = -1) {
        DescriptorPack pack {};

        // Check max sets
        if(maxSets < 0) {
            maxSets = pipelineData.allDescSetLayouts.size();
        }

        // Create pool
        pack.pool = vkInitData.device().createDescriptorPool(
                                vk::DescriptorPoolCreateInfo()
                                    .setPoolSizes(poolSizes)                    
                                    .setMaxSets(maxSets));

        // Create sets
        pack.sets = vkInitData.device().allocateDescriptorSets(
			                    vk::DescriptorSetAllocateInfo()
        		                    .setDescriptorPool(pack.pool)
        		                    .setDescriptorSetCount(pipelineData.allDescSetLayouts.size())
        		                    .setSetLayouts(pipelineData.allDescSetLayouts));

        // Return pack
        return pack;
    };

    inline void cleanupDescriptorPack(VulkanInitData &vkInitData, DescriptorPack &pack) {
        // Only have to cleanup the pool
        vkInitData.device().destroyDescriptorPool(pack.pool);
        pack = {};
    };

    inline void updateBufferForDescriptorPack(  VulkanInitData &vkInitData, 
                                                DescriptorPack &pack,
                                                VulkanBuffer &vbuffer,
                                                int setIndex,
                                                int bindingNum,
                                                vk::DescriptorType descType = vk::DescriptorType::eUniformBuffer,
                                                vk::DeviceSize offset = 0,
                                                int descCnt = 1) {

        vector<vk::WriteDescriptorSet> writes;            
        vk::DescriptorBufferInfo bufferInfo = vk::DescriptorBufferInfo()
                                                        .setBuffer(vbuffer.buffer)
                                                        .setOffset(offset)
                                                        .setRange(vbuffer.size);
        vk::WriteDescriptorSet descWrite = vk::WriteDescriptorSet()
            .setDstSet(pack.sets[setIndex])
            .setDstBinding(bindingNum)
            .setDstArrayElement(0)
            .setDescriptorType(descType)
            .setDescriptorCount(descCnt)
            .setBufferInfo(bufferInfo);

        writes.push_back(descWrite);    
        vkInitData.device().updateDescriptorSets(writes, {}); 
    };
}
