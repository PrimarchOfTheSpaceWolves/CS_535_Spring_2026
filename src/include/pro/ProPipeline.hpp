#pragma once
#include "ProSetup.hpp"

namespace pro {

    ///////////////////////////////////////////////////////////////////////////
    // HELPER FUNCTIONS
    ///////////////////////////////////////////////////////////////////////////

    inline vk::Viewport makeDefaultViewport(VulkanInitData &vkInitData, 
                                            bool flipViewportY = true) { 
        vk::Viewport viewport {};
        float width = (float)vkInitData.swapchain().extent.width;
        float height = (float)vkInitData.swapchain().extent.height;

        if(!flipViewportY) {
            // Y still downward
            // Will make problems for winding order
            viewport = vk::Viewport (0, 
                                     0, 
                                     width, 
                                     height, 
                                     0.0f, 1.0f);
        }
        else {
            // Y upward now
            viewport = vk::Viewport (0, 
                                     height,        // Start at bottom
                                     width, 
                                     -height,       // Using NEGATIVE height
                                     0.0f, 1.0f);
        }

        return viewport;
    };

    inline vk::Rect2D makeDefaultScissors(VulkanInitData &vkInitData) {
        return vk::Rect2D({0,0}, vkInitData.swapchain().extent);
    };

    ///////////////////////////////////////////////////////////////////////////
    // STRUCTS 
    ///////////////////////////////////////////////////////////////////////////

    struct VulkanShaderCreateInfo {
        string filename {};
        vk::ShaderStageFlagBits stage {};
        
        VulkanShaderCreateInfo(string filename, vk::ShaderStageFlagBits stage) {
            this->filename = filename;
            this->stage = stage;
        };
    };

    struct VulkanPipelineCreateInfo {
        // Shaders
        vector<VulkanShaderCreateInfo> shaderInfo {};
        
        // Vertex data
        vk::VertexInputBindingDescription bindDesc {};
        vector<vk::VertexInputAttributeDescription> attribDesc {};

        // Assembly type
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo {};

        // Dynamic rendering info
        vk::Format colorFormat {};
        vk::PipelineRenderingCreateInfo renderInfo {};

        // Uniform and layout info
        vector<vk::PushConstantRange> pushConstantRanges {};
        vector<vk::DescriptorSetLayout> allDescSetLayouts {}; 
        
        // Viewport and scissors
        vk::Viewport viewport {};
        vk::Rect2D scissor {};

        // Rasterizer info
        vk::PipelineRasterizationStateCreateInfo rasterizerInfo {};        
        
        // Color blend info   
        vk::PipelineColorBlendAttachmentState colorBlendAttachment {};     
        vk::PipelineColorBlendStateCreateInfo colorBlendInfo {};

        // Depth and stencil info
        vk::PipelineDepthStencilStateCreateInfo depthStencilInfo {};

        // MSAA info
        vk::PipelineMultisampleStateCreateInfo multisampleInfo {};
        
        // No-arg constructor
        VulkanPipelineCreateInfo(VulkanInitData &vkInitData, bool flipViewportY = true) {
            // Assembly defaults to triangle list
            inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo({}, vk::PrimitiveTopology::eTriangleList, false);

            // Default rendering info            
            renderInfo.colorAttachmentCount = 1;  
            colorFormat = vkInitData.swapchain().format;   
            renderInfo.pColorAttachmentFormats = &colorFormat; 
            renderInfo.depthAttachmentFormat = vk::Format::eD32Sfloat; 
            
            // Default rasterizer options            
            rasterizerInfo.lineWidth = 1.0f;
            rasterizerInfo.cullMode = vk::CullModeFlagBits::eBack;
            rasterizerInfo.frontFace = vk::FrontFace::eCounterClockwise; 

            // Default viewport and scissors
            viewport = makeDefaultViewport(vkInitData, flipViewportY);
            scissor = makeDefaultScissors(vkInitData);
            
            // Default color blend info (basically no color blending)            
            colorBlendAttachment.colorWriteMask = 
                vk::ColorComponentFlagBits::eR 
                | vk::ColorComponentFlagBits::eG 
                | vk::ColorComponentFlagBits::eB 
                | vk::ColorComponentFlagBits::eA;
            colorBlendInfo = vk::PipelineColorBlendStateCreateInfo({}, false, vk::LogicOp::eCopy, colorBlendAttachment);

            // Default depth testing info
            depthStencilInfo = vk::PipelineDepthStencilStateCreateInfo(
                                {},
                                true,                   // Enable depth testing    
                                true,                   // Enable depth writing    
                                vk::CompareOp::eLess,   // Lower depth = closer = keep
                                false,                  // Not putting bounds on depth test
                                false, {}, {}           // Not using stencil test
            );

            // Default MSAA info (basically off)
            multisampleInfo = vk::PipelineMultisampleStateCreateInfo({}, vk::SampleCountFlagBits::e1);         
        };
    };

    struct VulkanPipelineData {
        vk::PipelineCache cache;
        vk::PipelineLayout layout;
        vk::Pipeline pipeline;
        vector<vk::DescriptorSetLayout> allDescSetLayouts {};        
    };

    ///////////////////////////////////////////////////////////////////////////
    // FUNCTIONS 
    ///////////////////////////////////////////////////////////////////////////

    inline vector<char> readBinaryFile(const string& filename) {
        ifstream file(filename, ios::ate | ios::binary);

        if (!file.is_open()) {
            throw runtime_error("Failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    inline vk::ShaderModule createVulkanShaderModule(VulkanInitData &vkInitData, const vector<char>& code) {
        return vkInitData.device().createShaderModule(vk::ShaderModuleCreateInfo(
            vk::ShaderModuleCreateFlags(), code.size(), 
            reinterpret_cast<const uint32_t*>(code.data()) 
            // Cast that pretends as if it were a uint32_t pointer
        ));
    };

    inline void cleanupVulkanShaderModule(VulkanInitData &vkInitData, vk::ShaderModule &shaderModule) {
        vkInitData.device().destroyShaderModule(shaderModule);
    };

    inline VulkanPipelineData createVulkanPipeline( VulkanInitData &vkInitData, 
                                                    VulkanPipelineCreateInfo &creationInfo) {

        // Create data struct
        VulkanPipelineData data;

        // Shaders        
        vector<vk::ShaderModule> shaderModules {};
        vector<vk::PipelineShaderStageCreateInfo> shaderStages {};

        for(auto shaderData : creationInfo.shaderInfo) {
            auto shaderCode = readBinaryFile(shaderData.filename);
            vk::ShaderModule shaderMod = createVulkanShaderModule(vkInitData, shaderCode);
            shaderModules.push_back(shaderMod);

            vk::PipelineShaderStageCreateInfo shaderStageInfo(
                {}, shaderData.stage, shaderMod, "main");  
            shaderStages.push_back(shaderStageInfo);
        }

        // Vertex information
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
            {}, creationInfo.bindDesc, creationInfo.attribDesc);
        
        // Set viewport and scissor info
        vk::PipelineViewportStateCreateInfo viewportStateInfo({}, creationInfo.viewport, creationInfo.scissor);

        // Set dynamic state info
        vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor        
        };
        vk::PipelineDynamicStateCreateInfo dynamicStateInfo({}, dynamicStates);  

        // Set all layout info
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
            {}, 
            creationInfo.allDescSetLayouts,
            creationInfo.pushConstantRanges);
        data.allDescSetLayouts = creationInfo.allDescSetLayouts;

        // Create the layouts
        data.layout = vkInitData.device().createPipelineLayout(pipelineLayoutInfo);

        // Create the cache
        data.cache = vkInitData.device().createPipelineCache(vk::PipelineCacheCreateInfo());    

        // Create the master info
        vk::GraphicsPipelineCreateInfo pinfo {};
        pinfo.setFlags(vk::PipelineCreateFlags());
        pinfo.setStages(shaderStages);
        pinfo.setPVertexInputState(&vertexInputInfo);
        pinfo.setPInputAssemblyState(&(creationInfo.inputAssemblyInfo));
        pinfo.setPViewportState(&viewportStateInfo);
        pinfo.setPRasterizationState(&(creationInfo.rasterizerInfo));
        pinfo.setPMultisampleState(&(creationInfo.multisampleInfo));
        pinfo.setPDepthStencilState(&(creationInfo.depthStencilInfo));
        pinfo.setPColorBlendState(&(creationInfo.colorBlendInfo));
        pinfo.setPDynamicState(&dynamicStateInfo);
        pinfo.setLayout(data.layout);
        pinfo.setPNext(&(creationInfo.renderInfo));
        pinfo.setRenderPass(nullptr);        
        auto ret = vkInitData.device().createGraphicsPipeline(data.cache, pinfo);

        // Did we create the pipeline?
        if (ret.result != vk::Result::eSuccess) {
            throw runtime_error("Failed to create graphics pipeline!");
        }

        // Set pipeline
        data.pipeline = ret.value;
        
        // Cleanup modules
        for(auto shaderMod : shaderModules) {
            vkInitData.device().destroyShaderModule(shaderMod);
        }
        
        // Return data
        return data;
    };      

    inline void cleanupVulkanPipeline(VulkanInitData &vkInitData, VulkanPipelineData &pipelineData) {        
        for(int i = 0; i < pipelineData.allDescSetLayouts.size(); i++) {
            vkInitData.device().destroyDescriptorSetLayout(pipelineData.allDescSetLayouts.at(i));
        }
        pipelineData.allDescSetLayouts.clear();

        vkInitData.device().destroyPipelineCache(pipelineData.cache);
        vkInitData.device().destroyPipelineLayout(pipelineData.layout);
        vkInitData.device().destroyPipeline(pipelineData.pipeline);
    };
}
