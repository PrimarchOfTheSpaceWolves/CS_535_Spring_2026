#include <iostream>
#include <string>
#include "pro/Prometheus.hpp"
using namespace std;

bool didWindowResize = false;
glm::mat4 modelMat(1.0);

struct ForgeVertex {
    glm::vec3 pos = glm::vec3(0,0,0);
    glm::vec4 color = glm::vec4(1,1,1,1);
    glm::vec3 normal = glm::vec3(0,0,0);

    ForgeVertex() {};
    ForgeVertex(glm::vec3 p) { pos = p; };
    ForgeVertex(glm::vec3 p, glm::vec4 c) { pos = p; color = c; };
};

struct UniformPush {
    alignas(16) glm::mat4 modelMat;	
};

struct UBOVertex {
    alignas(16) glm::mat4 viewMat;
    alignas(16) glm::mat4 projMat;
};

struct alignas(16) PointLight {
    glm::vec4 pos;    
    glm::vec4 color;
};

struct alignas(16) UBOFragment {
    glm::vec4 cameraPos;
    alignas(16) uint32_t lightCnt;
};

glm::vec3 cameraPos(-1,1,1);
UBOVertex uboVertHost {};
UBOFragment uboFragHost {};
vector<PointLight> hostLights {};

void printRM(string name, glm::mat4 &M) {
    cout << name << ":" << endl;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            cout << M[j][i] << ", ";
        }
        cout << endl;
    }
}

void computeOneNormal(pro::HostMesh<ForgeVertex> &m, int i1, int i2, int i3) {
    auto v1 = m.vertices[i1];
    auto v2 = m.vertices[i2];
    auto v3 = m.vertices[i3];
    auto s1 = v2.pos - v1.pos;
    auto s2 = v3.pos - v1.pos;
    auto n = glm::normalize(glm::cross(s1, s2));
    m.vertices[i1].normal += n;
    m.vertices[i2].normal += n;
    m.vertices[i3].normal += n;
}

void computeAllNormals(pro::HostMesh<ForgeVertex> &m) {
    for(int i = 0; i < m.indices.size(); i += 3) {
        computeOneNormal(m, m.indices[i], m.indices[i+1], m.indices[i+2]);
    }

    for(int i = 0; i < m.vertices.size(); i++) {
        m.vertices[i].normal = glm::normalize(m.vertices[i].normal);
    }
}

pro::HostMesh<ForgeVertex> makeCylinder(float length, float radius, int faceCnt) {
    pro::HostMesh<ForgeVertex> m {};
    float angleInc = 2.0f*glm::pi<float>()/((float)faceCnt);

    for(int i = 0; i < faceCnt; i++) {
        float sinVal = glm::sin(angleInc*i);
        float cosVal = glm::cos(angleInc*i);

        ForgeVertex v1 = ForgeVertex({-length/2.0f, radius*sinVal, radius*cosVal}, {1,0,0,1});
        ForgeVertex v2 = ForgeVertex({+length/2.0f, radius*sinVal, radius*cosVal}, {0,1,0,1});

        m.vertices.push_back(v1);
        m.vertices.push_back(v2);

        if(i < (faceCnt-1)) {
            int baseIndex = i*2;
            m.indices.push_back(baseIndex);
            m.indices.push_back(baseIndex+1);
            m.indices.push_back(baseIndex+2);
            
            m.indices.push_back(baseIndex+1);
            m.indices.push_back(baseIndex+3);
            m.indices.push_back(baseIndex+2);
        }
    }

    int lastIndex = m.vertices.size()-1;
    m.indices.push_back(lastIndex-1);
    m.indices.push_back(lastIndex);
    m.indices.push_back(0);

    m.indices.push_back(lastIndex);
    m.indices.push_back(1);
    m.indices.push_back(0);

    computeAllNormals(m);

    return m;    
}

static void window_resize_callback(GLFWwindow* window, int width, int height) {
    didWindowResize = true;    
}

static void key_callback(GLFWwindow* window, 
							int key, int scancode, 
							int action, int mods) {

	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if(key == GLFW_KEY_ESCAPE) {
		    glfwSetWindowShouldClose(window, true);
        }
        else if(key == GLFW_KEY_Q) {
            modelMat = glm::rotate(glm::radians(5.0f), glm::vec3(0,0,1)) * modelMat;
        }
        else if(key == GLFW_KEY_E) {
            modelMat = glm::rotate(glm::radians(-5.0f), glm::vec3(0,0,1)) * modelMat;
        }
        else if(key == GLFW_KEY_SPACE) {
            modelMat = glm::mat4(1.0);
        }
        else if(key == GLFW_KEY_Z) {
            modelMat = glm::rotate(glm::radians(5.0f), glm::vec3(0, 1, 0)) * modelMat;
        }
	}
}

int main(int argc, char **argv) {
    cout << "BEGIN VULKAN EXERCISE" << endl;

    string appName = "ProfExercises13";
    string windowName = appName + ": <Your SITNET ID>";

    if(!glfwInit()) {
        cerr << "ERROR: Cannot start GLFW!" << endl;
        exit(1);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, true);
    GLFWwindow *window = glfwCreateWindow(800, 600, windowName.c_str(), nullptr, nullptr);

    if(!window) {
        cerr << "ERROR: Cannot create GLFW window!" << endl;
        glfwTerminate();
        exit(1);
    }

    glfwSetFramebufferSizeCallback(window, window_resize_callback);
    glfwSetKeyCallback(window, key_callback);


    {
        pro::VulkanInitCreateInfo createInfo {};
        createInfo.appName = appName;
        //createInfo.requestedAppVulkanVersionMinor = 3;        
        //createInfo.requireComputeQueue = false;
        //createInfo.requireTransferQueue = false;
        createInfo.createSurfaceFunc = [window](VkInstance instance, VkSurfaceKHR& surface) {            
            return glfwCreateWindowSurface(instance, window, nullptr, &surface);
        };
        createInfo.getCurrentWindowSizeFunc = [window](int &width, int &height) {
            glfwGetFramebufferSize(window, &width, &height);
        };
    
        pro::VulkanInitData vkInitData(createInfo);

        pro::FrameCommandData commandData = pro::createFrameCommandData(vkInitData);
        uint32_t framesRendered = 0;
        int numberOfFramesInFlight = 1;

        pro::listAvailablePhysicalDevices(vkInitData.instance());

        cout << "** Chosen Physical Device: *********" << endl;
        pro::printPhysicalDeviceProperties(vkInitData.physicalDevice());

        vector<pro::VulkanImage> allDepthImages {};
        pro::recreateAllVulkanDepthImages(vkInitData, allDepthImages, numberOfFramesInFlight);

        pro::OnResizeFunc resizeFunc = [&vkInitData, window, &allDepthImages, &numberOfFramesInFlight]() {            
            int width = 0;
            int height = 0;
            do {
                glfwGetFramebufferSize(window, &width, &height);                
                glfwWaitEvents(); 
            } while (width == 0 || height == 0);          
            vkInitData.recreateVulkanSwapchain();     
            pro::recreateAllVulkanDepthImages(vkInitData, allDepthImages, numberOfFramesInFlight);   
            cout << "Swapchain recreated..." << endl;
        };

        vk::QueryPoolCreateInfo qpCI{};
        qpCI.queryType = vk::QueryType::eTimestamp;
        qpCI.queryCount = 2;
        vk::QueryPool queryPool = vkInitData.device().createQueryPool(qpCI);

        // Add host lights
        hostLights.push_back({{0, 0.5, 0.5, 1.0}, {0,1,0,1}});
        hostLights.push_back({{0, 0.5, -0.5, 1.0}, {1,0,0,1}});

        // Create SSBOs and copy lights in
        vector<pro::VulkanBuffer> ssboLights {};
        ssboLights.resize(numberOfFramesInFlight);  
        for (unsigned int i = 0; i < numberOfFramesInFlight; i++) {
            ssboLights[i] = pro::createVulkanBuffer(   vkInitData, sizeof(PointLight)*hostLights.size(),
                                                        vk::BufferUsageFlagBits::eStorageBuffer, 
                                                        pro::createVMAHostVisibleInfo());
            pro::copyToHostVisibleVulkanBuffer(vkInitData, ssboLights[i], hostLights.data());
        }

        // Create UBOs for fragment shader
        vector<pro::VulkanBuffer> uboFragData {};
        uboFragData.resize(numberOfFramesInFlight);  
        for (unsigned int i = 0; i < numberOfFramesInFlight; i++) {
            uboFragData[i] = pro::createVulkanBuffer(vkInitData, sizeof(UBOFragment),
                                                        vk::BufferUsageFlagBits::eUniformBuffer,
                                                        pro::createVMAHostVisibleInfo());
        }


        pro::VulkanPipelineCreateInfo pipelineCreateInfo(vkInitData);

        // Create shader info
        pipelineCreateInfo.shaderInfo = {
            pro::VulkanShaderCreateInfo(
                "build/compiledshaders/" + appName + "/shader.vert.spv",
                vk::ShaderStageFlagBits::eVertex
            ),

            pro::VulkanShaderCreateInfo(
                "build/compiledshaders/" + appName + "/shader.frag.spv",
                vk::ShaderStageFlagBits::eFragment
            )
        };

        pipelineCreateInfo.bindDesc = vk::VertexInputBindingDescription(
            0, sizeof(ForgeVertex), vk::VertexInputRate::eVertex);

        // POSITION
        pipelineCreateInfo.attribDesc.push_back(vk::VertexInputAttributeDescription(
            0, // location
            0, // binding
            vk::Format::eR32G32B32Sfloat,  // format
            offsetof(ForgeVertex, pos) // offset
        ));

        // COLOR
        pipelineCreateInfo.attribDesc.push_back(vk::VertexInputAttributeDescription(
            1, // location
            0, // binding
            vk::Format::eR32G32B32A32Sfloat,  // format
            offsetof(ForgeVertex, color) // offset
        ));

        pipelineCreateInfo.attribDesc.push_back(vk::VertexInputAttributeDescription(
		2,0,vk::Format::eR32G32B32Sfloat, offsetof(ForgeVertex, normal)));


        pipelineCreateInfo.pushConstantRanges = { 
            {vk::ShaderStageFlagBits::eVertex, 0, sizeof(UniformPush)}
        };

        vector<vk::DescriptorSetLayoutBinding> allBindings = {
            vk::DescriptorSetLayoutBinding(
                0, vk::DescriptorType::eUniformBuffer, 
                1, vk::ShaderStageFlagBits::eVertex),
            vk::DescriptorSetLayoutBinding(
                1, vk::DescriptorType::eUniformBuffer, 
                1, vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding(
                2, vk::DescriptorType::eStorageBuffer, 
                1, vk::ShaderStageFlagBits::eFragment)
        };
            
 
        pipelineCreateInfo.allDescSetLayouts.push_back(
            vkInitData.device().createDescriptorSetLayout(
                vk::DescriptorSetLayoutCreateInfo({}, 
                    allBindings))
        );

        pipelineCreateInfo.rasterizerInfo.cullMode = vk::CullModeFlagBits::eNone;

        pro::VulkanPipelineData pipelineData = createVulkanPipeline(vkInitData, pipelineCreateInfo);

        pro::TransferManager transferMgr = pro::TransferManager(vkInitData);  
        vector<pro::HostMesh<ForgeVertex>> allHostMeshes {};      
        vector<pro::VulkanMesh> allMeshes {}; 
        vector<pro::VulkanMesh> waitingMeshes {};
        vector<pro::VulkanMesh> readyToRenderMeshes {};          
        vector<pro::PendingBufferCopy> pendingCopies {};
        vector<pro::BufferCopyReceipt*> receiptsToCheck {};       
        unordered_map<string, vector<pro::VulkanMesh>> receiptToData {};

        bool DO_ASYNC_COPY = true;

        /*
        pro::HostMesh<ForgeVertex> firstHost {};
        firstHost.vertices = {
            {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}}
        };
        firstHost.indices = { 0, 1, 2, 2, 3, 0 };
        allHostMeshes.push_back(firstHost);
        
        pro::HostMesh<ForgeVertex> secondHost {};
        secondHost.vertices = {
            {{-0.3f, -0.4f, 0.7f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{0.7f, -0.4f, 0.7f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{0.7f, 0.4f, 0.7f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{-0.3f, 0.4f, 0.7f}, {0.0f, 1.0f, 0.0f, 1.0f}}
        };
        secondHost.indices = { 0, 1, 2, 2, 3, 0 };
        allHostMeshes.push_back(secondHost);
        */

        pro::HostMesh<ForgeVertex> hostCylinder = makeCylinder(1.0, 0.5, 10);
        allHostMeshes.push_back(hostCylinder);
        
        allMeshes.resize(allHostMeshes.size()); 
        
        for(int i = 0; i < allHostMeshes.size(); i++) {
            allMeshes[i] = createVulkanMesh(vkInitData, allHostMeshes[i], true);                    
            pro::addPendingBufferCopies(allMeshes[i], allHostMeshes[i], pendingCopies);
            waitingMeshes.push_back(allMeshes[i]);
        }

        auto meshReceipt = transferMgr.submitCopies("AllMeshes", pendingCopies);
        receiptsToCheck.push_back(meshReceipt);
        receiptToData[meshReceipt->getCopyID()] = waitingMeshes;
        pendingCopies.clear();               
            
        if(!DO_ASYNC_COPY) {
            vector<string> receiptsDone {};   
            transferMgr.waitUntilCompleted(receiptsToCheck, receiptsDone);

            for(auto it = receiptsDone.begin(); it != receiptsDone.end(); it++) {
                vector<pro::VulkanMesh> newMeshes = receiptToData[*it];
                readyToRenderMeshes.insert(readyToRenderMeshes.end(),
                                           newMeshes.begin(), newMeshes.end());
            }
        }

        // UBO creation
        vector<pro::VulkanBuffer> uboVertData {};
        uboVertData.resize(numberOfFramesInFlight);  
        for (unsigned int i = 0; i < numberOfFramesInFlight; i++) {
            uboVertData[i] = pro::createVulkanBuffer(vkInitData, sizeof(UBOVertex),
                                                        vk::BufferUsageFlagBits::eUniformBuffer,
                                                        pro::createVMAHostVisibleInfo());
        }

        
        vector<pro::DescriptorPack> allDescPacks {};
        allDescPacks.resize(numberOfFramesInFlight);

        vector<vk::DescriptorPoolSize> poolSizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1)
        };

        for(int i = 0; i < allDescPacks.size(); i++) {
            allDescPacks[i] = pro::createDescriptorPack(vkInitData, pipelineData, poolSizes);       
            pro::updateBufferForDescriptorPack(vkInitData, allDescPacks[i],
                                                uboVertData[i], 0, 0);   
            pro::updateBufferForDescriptorPack(vkInitData, allDescPacks[i],
                                                uboFragData[i], 0, 1);   
            pro::updateBufferForDescriptorPack(vkInitData, allDescPacks[i],
                                                ssboLights[i], 0, 2,
                                                vk::DescriptorType::eStorageBuffer);                  
        }

        cout << "Starting render loop..." << endl;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if(didWindowResize) {
                didWindowResize = false;
                resizeFunc();
            }

            unsigned int indexFlight = framesRendered % numberOfFramesInFlight;
            unsigned int indexSwap = pro::acquireNextSwapImage(vkInitData, commandData, resizeFunc);
            
            // TODO
            vkInitData.device().resetCommandPool(commandData.commandPool);
            commandData.commandBuffer.begin(vk::CommandBufferBeginInfo());

            // Reset query pool and write a time stamp to query 0
            commandData.commandBuffer.resetQueryPool(queryPool, 0, 2);
            commandData.commandBuffer.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, 
                                                        queryPool, 0); 
            
            if(DO_ASYNC_COPY) {
                vector<string> receiptsDone {};   
                if(transferMgr.checkAnyCompleted(receiptsToCheck, commandData.commandBuffer, receiptsDone)) {

                    for(auto it = receiptsDone.begin(); it != receiptsDone.end(); it++) {
                        vector<pro::VulkanMesh> newMeshes = receiptToData[*it];
                        readyToRenderMeshes.insert(readyToRenderMeshes.end(),
                                                newMeshes.begin(), newMeshes.end());
                    }

                    cout << "Meshes copied." << endl;
                }               
            }

            // Transition swap image from undefined to color buffer
            pro::performVulkanImageTransition(commandData.commandBuffer, 
                                        vkInitData.swapchain().swaps[indexSwap].image, 
                                        pro::IMAGE_TRANSITION_TYPE::UNDEF_TO_COLOR);

            vk::RenderingAttachmentInfoKHR colorAtt
             = pro::createColorAttachment(
                vkInitData.swapchain().swaps[indexSwap].view, 
                vk::ClearColorValue {1.0f, 1.0f, 0.0f, 1.0f});

            vk::RenderingAttachmentInfoKHR depthAtt
            = pro::createDepthAttachment(allDepthImages[indexFlight].view);

            vk::RenderingInfoKHR ri{};
            ri.setRenderArea(vk::Rect2D{ {0,0}, vkInitData.swapchain().extent })
                .setLayerCount(1)
                .setColorAttachments(colorAtt)
                .setPDepthAttachment(&depthAtt);

            commandData.commandBuffer.beginRendering(ri);
            commandData.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineData.pipeline);
            vk::Viewport viewports[] = { pro::makeDefaultViewport(vkInitData) };    
            commandData.commandBuffer.setViewport(0, viewports);
    
            vk::Rect2D scissors[] = { pro::makeDefaultScissors(vkInitData) };
            commandData.commandBuffer.setScissor(0, scissors);

            // OBJECT RENDERING HERE

            UniformPush pv = {};
            pv.modelMat = modelMat;
                                
            commandData.commandBuffer.pushConstants(
                pipelineData.layout,                  	  // Pipeline layout
                vk::ShaderStageFlagBits::eVertex,           // Stage flags
                0,                                                                    // Offset
                sizeof(UniformPush),                                  // Size
                &pv                                                                // Pointer to data
            ); 
            

            uboVertHost.viewMat = glm::lookAt(  cameraPos, 
                                                glm::vec3(0, 0, 0), 
                                                glm::vec3(0, 1, 0));

            float fov = glm::radians(90.0f);
            float aspectRatio = ((float) vkInitData.swapchain().extent.width) / ((float) vkInitData.swapchain().extent.height);
            float near = 0.01;
            float far = 1000.0;
            uboVertHost.projMat = glm::perspective(fov, aspectRatio, near, far);

            pro::copyToHostVisibleVulkanBuffer(vkInitData, 
                                                uboVertData[indexFlight], 
                                                &uboVertHost); 

            uboFragHost.cameraPos = glm::vec4(cameraPos, 1.0f);
            uboFragHost.lightCnt = hostLights.size();
            pro::copyToHostVisibleVulkanBuffer(vkInitData, 
                                                uboFragData[indexFlight], 
                                                &uboFragHost); 

    
            commandData.commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, 
                pipelineData.layout,
                0, allDescPacks[indexFlight].sets, {}); 
            
            for(int i = 0; i < readyToRenderMeshes.size(); i++) {
                pro::recordDrawVulkanMesh(commandData.commandBuffer, readyToRenderMeshes[i]);
            }

            commandData.commandBuffer.endRendering();

            // Transition swap image from color buffer to presentation
            pro::performVulkanImageTransition(commandData.commandBuffer, 
                                            vkInitData.swapchain().swaps[indexSwap].image,
                                            pro::IMAGE_TRANSITION_TYPE::COLOR_TO_PRESENT);
    

            // Write the second time stamp
            commandData.commandBuffer.writeTimestamp2(
                vk::PipelineStageFlagBits2::eBottomOfPipe, queryPool, 1);

            // End recording
            commandData.commandBuffer.end();

            pro::submitToGraphicsQueue(vkInitData, commandData, indexSwap, resizeFunc);
            if(!pro::presentSwapImage(vkInitData, commandData, indexSwap, resizeFunc)) {
                cerr << "Warning: Presentation was not successful." << endl;
            }
            framesRendered++;

            /*
            uint64_t timestamps[2] = {};
            vkInitData.device().getQueryPoolResults(queryPool, 
                                                    0, 2,
                                                    sizeof(timestamps), 
                                                    timestamps,
                                                    sizeof(uint64_t),
                                                    vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);

            // Convert ticks to nanoseconds 
            auto props = vkInitData.physicalDevice().getProperties();
            double nsPerTick = props.limits.timestampPeriod; 
            double deltaNs = (timestamps[1] - timestamps[0]) * nsPerTick;
            //cout << "TIME for FRAME: " << deltaNs << endl;
            */
        }

        vkInitData.device().waitIdle();

        for(int i = 0; i < allMeshes.size(); i++) {
            pro::cleanupVulkanMesh(vkInitData, allMeshes[i]);
        }
        allMeshes.clear();        
        readyToRenderMeshes.clear();
        receiptsToCheck.clear();

        pro::cleanupAllVulkanDepthImages(vkInitData, allDepthImages);

        for(unsigned int i = 0; i < numberOfFramesInFlight; i++) {
            pro::cleanupVulkanBuffer(vkInitData, uboVertData[i]);
            pro::cleanupVulkanBuffer(vkInitData, uboFragData[i]);
            pro::cleanupVulkanBuffer(vkInitData, ssboLights[i]);
        }
        uboVertData.clear();
        uboFragData.clear();
        ssboLights.clear();

        for(unsigned int i = 0; i < allDescPacks.size(); i++) {
            pro::cleanupDescriptorPack(vkInitData, allDescPacks[i]);            
        }
        allDescPacks.clear();

        cleanupVulkanPipeline(vkInitData, pipelineData);
        vkInitData.device().destroyQueryPool(queryPool);
        pro::cleanupFrameCommandData(vkInitData, commandData); 
    }

    glfwDestroyWindow(window);
    glfwTerminate();  

    return 0;
}
