#pragma once
#include "ProBuffer.hpp"

namespace pro {
    ///////////////////////////////////////////////////////////////////////////
    // STRUCTS 
    ///////////////////////////////////////////////////////////////////////////
        
    template<typename T>
    struct HostMesh {
        vector<T> vertices {};
        vector<unsigned int> indices {};
    };
    
    struct VulkanMesh {
        VulkanBuffer vertices;
        VulkanBuffer indices;
        unsigned int indexCnt = 0;
    };
        
    ///////////////////////////////////////////////////////////////////////////
    // FUNCTIONS 
    ///////////////////////////////////////////////////////////////////////////  

    template<typename T>
    VulkanMesh createVulkanMesh(    VulkanInitData &vkInitData,                                     
                                    HostMesh<T> &hostMesh,
                                    bool isDeviceLocal) {
        // Set up Vulkan mesh                            
        VulkanMesh mesh;

        // Create appropriate VMA info and usage flags
        VmaAllocationCreateInfo vmaInfo {};
        vk::BufferUsageFlags vertUsageFlags = vk::BufferUsageFlagBits::eVertexBuffer;
        vk::BufferUsageFlags indexUsageFlags = vk::BufferUsageFlagBits::eIndexBuffer;

        if(isDeviceLocal) {
            vmaInfo = createVMADeviceLocalInfo();
            vertUsageFlags |= vk::BufferUsageFlagBits::eTransferDst;
            indexUsageFlags |= vk::BufferUsageFlagBits::eTransferDst;
        }
        else {
            vmaInfo = createVMAHostVisibleInfo();
        }

        // Create vertex buffer and index buffer
        vk::DeviceSize vertBufferSize = sizeof(hostMesh.vertices[0]) * hostMesh.vertices.size();    
        mesh.vertices = createVulkanBuffer(vkInitData, vertBufferSize, vertUsageFlags, vmaInfo);

        vk::DeviceSize indexBufferSize = sizeof(hostMesh.indices[0]) * hostMesh.indices.size();
        mesh.indices = createVulkanBuffer(vkInitData, indexBufferSize, indexUsageFlags, vmaInfo);

        // Return mesh
        return mesh;
    };

    template<typename T>
    void copyToHostVisibleVulkanMesh(   VulkanInitData &vkInitData,  
                                        VulkanMesh &mesh,                                   
                                        HostMesh<T> &hostMesh) {
        
        // Copy to buffers
        copyToHostVisibleVulkanBuffer(vkInitData, mesh.vertices, hostMesh.vertices.data());
        copyToHostVisibleVulkanBuffer(vkInitData, mesh.indices, hostMesh.indices.data());
        
        // Set index count
        mesh.indexCnt = hostMesh.indices.size();
    };

    template<typename T>
    void addPendingBufferCopies (   VulkanMesh &mesh,                                   
                                    HostMesh<T> &hostMesh,
                                    vector<PendingBufferCopy> &pendingCopies) {

        pendingCopies.push_back(PendingBufferCopy(mesh.vertices, hostMesh.vertices.data(), vk::AccessFlagBits::eVertexAttributeRead));
        pendingCopies.push_back(PendingBufferCopy(mesh.indices, hostMesh.indices.data(), vk::AccessFlagBits::eIndexRead));
        
        // Still set index count
        mesh.indexCnt = hostMesh.indices.size();
    };

    void recordDrawVulkanMesh(vk::CommandBuffer &commandBuffer, VulkanMesh &mesh) {
        
        vk::Buffer vertexBuffers[] = {mesh.vertices.buffer};
        vk::DeviceSize offsets[] = {0};
        commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
        commandBuffer.bindIndexBuffer(mesh.indices.buffer, 0, vk::IndexType::eUint32);
        
        commandBuffer.drawIndexed(mesh.indexCnt, 1, 0, 0, 0);
    };   

    void cleanupVulkanMesh(VulkanInitData &vkInitData, VulkanMesh &mesh) {
        cleanupVulkanBuffer(vkInitData, mesh.vertices);
        cleanupVulkanBuffer(vkInitData, mesh.indices);
    };
}
