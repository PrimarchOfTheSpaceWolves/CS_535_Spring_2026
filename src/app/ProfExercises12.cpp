#include <iostream>
#include <string>
#include "pro/Prometheus.hpp"
using namespace std;

bool didWindowResize = false;
glm::mat4 modelMat(1.0);

struct ForgeVertex {
    glm::vec3 pos;
    glm::vec4 color;
};

struct UniformPush {
    alignas(16) glm::mat4 modelMat;
};

struct UBOVertex {
    alignas(16) glm::mat4 viewMat {};
    alignas(16) glm::mat4 projMat {};
};

UBOVertex uboVertHost {};

static void window_resize_callback(GLFWwindow* window, int width, int height) {
    didWindowResize = true;
}

static void key_callback(   GLFWwindow *window,
                            int key,
                            int scancode,
                            int action,
                            int mods) {

    if(action == GLFW_PRESS || action == GLFW_REPEAT) {
        if(key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, true);
        }
        else if(key == GLFW_KEY_Q) {
            modelMat = glm::rotate(glm::radians(5.0f), glm::vec3(0,0,1)) * modelMat;
        }
        else if(key == GLFW_KEY_W) {
            modelMat = glm::translate(glm::vec3(0,0.1,0))*modelMat;
        }
        else if(key == GLFW_KEY_SPACE) {
            modelMat = glm::mat4(1.0);
        }
    }
}

int main(int argc, char **argv) {
    cout << "Starting exercises!" << endl;

    string filename = "Nothing";
    if(argc >= 2) {
        filename = string(argv[1]);
    }
    // filename.c_str()
    cout << "Filename: " << filename << endl;


    glm::vec3 A = glm::vec3(1,4,0);
    glm::vec3 B = glm::vec3(2,3,2);

    cout << "A.x = " << A.x << endl;
    cout << "A = " << glm::to_string(A) << endl;
    cout << "B = " << glm::to_string(B) << endl;
    glm::vec3 C = B - A;
    cout << "C = " << glm::to_string(C) << endl;

    A = 5.0f*A;
    cout << "A = " << glm::to_string(A) << endl;

    glm::vec3 normA = glm::normalize(A);
    cout << "normA = " << glm::to_string(normA) << endl;

    cout << "Length A = " << glm::length(A) << endl;
    cout << "Length normA = " << glm::length(normA) << endl;

    glm::vec3 normB = glm::normalize(B);
    float dotAB = glm::dot(normA, normB);
    cout << "dotAB = " << dotAB << endl;









    if(!glfwInit()) {
        cerr << "FAILED TO INIT GLFW!" << endl;
        exit(1);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, true);

    string appName = "ProfExercises12";
    int winWidth = 800;
    int winHeight = 600;
    GLFWwindow *window = glfwCreateWindow(winWidth, winHeight, 
                                            appName.c_str(), 
                                            nullptr, nullptr);
    if(!window) {
        cerr << "FAILED TO CREATE WINDOW!" << endl;
        glfwTerminate();
        exit(1);
    }

    glfwSetFramebufferSizeCallback(window, window_resize_callback);
    glfwSetKeyCallback(window, key_callback);

    {
        pro::VulkanInitCreateInfo initCreateInfo {};
        initCreateInfo.appName = appName;

        initCreateInfo.createSurfaceFunc = [window](VkInstance instance,
                                                    VkSurfaceKHR &surface) {
            return glfwCreateWindowSurface(instance, window, nullptr, &surface);
        };

        initCreateInfo.getCurrentWindowSizeFunc = [window](int &width, int &height) {
            glfwGetFramebufferSize(window, &width, &height);
        };

        initCreateInfo.requestedAppVulkanVersionMinor = 3;
        initCreateInfo.requireComputeQueue = false;
        initCreateInfo.requireTransferQueue = false;

        pro::VulkanInitData vkInitData(initCreateInfo);

        pro::listAvailablePhysicalDevices(vkInitData.instance());

        cout << "THE CHOSEN ONE:" << endl;
        pro::printPhysicalDeviceProperties(vkInitData.physicalDevice());

        int numberFramesInFlight = 1;

        vector<pro::VulkanImage> allDepthImages {};
        pro::recreateAllVulkanDepthImages(
            vkInitData, allDepthImages, 
            numberFramesInFlight);

        pro::OnResizeFunc resizeFunc = [&vkInitData, window,
                                        &allDepthImages,
                                        numberFramesInFlight]() {
            int width = 0;
            int height = 0;
            do {
                glfwGetFramebufferSize(window, &width, &height);
                glfwWaitEvents();
            } while(width == 0 || height == 0);
            vkInitData.recreateVulkanSwapchain();
            pro::recreateAllVulkanDepthImages(
                            vkInitData, allDepthImages, 
                            numberFramesInFlight);
            cout << "Swapchain recreated..." << endl;
        };

        pro::FrameCommandData commandData = pro::createFrameCommandData(vkInitData);
        uint32_t framesRendered = 0;
        

        vk::QueryPoolCreateInfo qpci {};
        qpci.queryType = vk::QueryType::eTimestamp;
        qpci.queryCount = 2;
        vk::QueryPool queryPool = vkInitData.device().createQueryPool(qpci);

        pro::VulkanPipelineCreateInfo pipelineCreateInfo(vkInitData);

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
            0, sizeof(ForgeVertex), vk::VertexInputRate::eVertex
        );

        pipelineCreateInfo.attribDesc.push_back(
            vk::VertexInputAttributeDescription(
                0, 0, vk::Format::eR32G32B32Sfloat, offsetof(ForgeVertex, pos)
            ));
        pipelineCreateInfo.attribDesc.push_back(   
            vk::VertexInputAttributeDescription(
                1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(ForgeVertex, color)
            ));

        pipelineCreateInfo.pushConstantRanges.push_back(
            {vk::ShaderStageFlagBits::eVertex, 0, sizeof(UniformPush)}
        );

        vector<pro::VulkanBuffer> uboVertData {};
        uboVertData.resize(numberFramesInFlight);
        for(int i = 0; i < numberFramesInFlight; i++) {
            uboVertData[i] = pro::createVulkanBuffer(vkInitData,
                                    sizeof(UBOVertex), 
                                    vk::BufferUsageFlagBits::eUniformBuffer,
                                    pro::createVMAHostVisibleInfo());
        }

        vector<vk::DescriptorSetLayoutBinding> allBindings = {
            vk::DescriptorSetLayoutBinding(
                0, vk::DescriptorType::eUniformBuffer,
                1, vk::ShaderStageFlagBits::eVertex
            )
        };
        pipelineCreateInfo.allDescSetLayouts.push_back(
            vkInitData.device().createDescriptorSetLayout(
                vk::DescriptorSetLayoutCreateInfo({},
                allBindings)
            )
        );

        pro::VulkanPipelineData pipelineData = pro::createVulkanPipeline(
                                                vkInitData, 
                                                pipelineCreateInfo);

        pro::TransferManager transferManager = pro::TransferManager(vkInitData);

        vector<pro::VulkanMesh> allMeshes {};
        vector<pro::VulkanMesh> waitingMeshes {};
        vector<pro::VulkanMesh> readyToRenderMeshes {};

        vector<pro::PendingBufferCopy> pendingCopies {};
        vector<pro::BufferCopyReceipt*> copiesToCheck {};

        pro::HostMesh<ForgeVertex> hostMesh {};
        hostMesh.vertices = {
            {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{+0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{+0.5f, +0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f, +0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}}
        };
        hostMesh.indices = {0,1,2,2,3,0};
        
        pro::VulkanMesh mesh = pro::createVulkanMesh(
                                        vkInitData,
                                        hostMesh,
                                        true);
        pro::addPendingBufferCopies(mesh, hostMesh, pendingCopies);
        auto transferReceipt = transferManager.submitCopies(
            "SquareMesh", pendingCopies
        );
        copiesToCheck.push_back(transferReceipt);
        pendingCopies.clear();

        allMeshes.push_back(mesh);
        waitingMeshes.push_back(mesh);


        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if(didWindowResize) {
                resizeFunc();
                didWindowResize = false;
            }

            unsigned int indexFlight = framesRendered % numberFramesInFlight;
            unsigned int indexSwap = pro::acquireNextSwapImage(vkInitData,
                                                                commandData, 
                                                                resizeFunc);
            
            vkInitData.device().resetCommandPool(commandData.commandPool);
            commandData.commandBuffer.begin(vk::CommandBufferBeginInfo());

            for(auto it = copiesToCheck.begin();
                it != copiesToCheck.end(); ) {

                if(transferManager.checkCompleted(
                    *it, commandData.commandBuffer)) {
                    readyToRenderMeshes.insert(
                        readyToRenderMeshes.end(),
                        waitingMeshes.begin(),
                        waitingMeshes.end()
                    );
                    waitingMeshes.clear();
                    it = copiesToCheck.erase(it);
                }
                else {
                    it++;
                }
            }



            //commandData.commandBuffer.resetQueryPool(queryPool, 0, 2);
            //commandData.commandBuffer.writeTimestamp2(
            //    vk::PipelineStageFlagBits2::eTopOfPipe, queryPool, 0);

            pro::performVulkanImageTransition(
                commandData.commandBuffer,
                vkInitData.swapchain().swaps[indexSwap].image,
                pro::IMAGE_TRANSITION_TYPE::UNDEF_TO_COLOR
            );

            auto colorAtt = pro::createColorAttachment(
                vkInitData.swapchain().swaps[indexSwap].view,
                vk::ClearColorValue(0.0f, 0.7f, 0.0f, 1.0f)
            );

            auto depthAtt = pro::createDepthAttachment(allDepthImages[indexFlight].view);

            vk::RenderingInfoKHR ri {};
            ri.setRenderArea(vk::Rect2D({0,0}, vkInitData.swapchain().extent))
                .setLayerCount(1)
                .setColorAttachments(colorAtt)
                .setPDepthAttachment(&depthAtt);

            commandData.commandBuffer.beginRendering(ri);
            commandData.commandBuffer.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                pipelineData.pipeline);

            vk::Viewport viewports [] = { pro::makeDefaultViewport(vkInitData)};
            vk::Rect2D scissors [] = { pro::makeDefaultScissors(vkInitData)};

            commandData.commandBuffer.setViewport(0, viewports);
            commandData.commandBuffer.setScissor(0, scissors);
            
            // TODO: This is where the rendering magic happens

            UniformPush pv = {};
            pv.modelMat = modelMat;

            commandData.commandBuffer.pushConstants(
                pipelineData.layout, vk::ShaderStageFlagBits::eVertex,
                0, sizeof(UniformPush), &pv
            );

            for(int i = 0; i < readyToRenderMeshes.size(); i++) {
                pro::recordDrawVulkanMesh(
                    commandData.commandBuffer,
                    readyToRenderMeshes[i]);
            }

            commandData.commandBuffer.endRendering();

            pro::performVulkanImageTransition(
                commandData.commandBuffer,
                vkInitData.swapchain().swaps[indexSwap].image,
                pro::IMAGE_TRANSITION_TYPE::COLOR_TO_PRESENT
            );

            //commandData.commandBuffer.writeTimestamp2(
            //    vk::PipelineStageFlagBits2::eBottomOfPipe, queryPool, 1);

            commandData.commandBuffer.end();

            pro::submitToGraphicsQueue(vkInitData, commandData, indexSwap, resizeFunc);

            if(!pro::presentSwapImage(vkInitData, commandData, indexSwap, resizeFunc)) {
                cerr << "Error: Presentation not successful!" << endl;
            }

            framesRendered++;

            /*
            uint64_t timestamps[2] = {};
            vkInitData.device().getQueryPoolResults(
                queryPool, 0, 2, sizeof(timestamps), timestamps, 
                sizeof(uint64_t), 
                vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
             
            auto props = vkInitData.physicalDevice().getProperties();
            double nsPerTick = props.limits.timestampPeriod;
            double deltaNs = (timestamps[1] - timestamps[0])*nsPerTick;
            cout << "TIME: " << deltaNs << endl;
            */
        }

        vkInitData.device().waitIdle();

        pro::cleanupAllVulkanDepthImages(vkInitData, allDepthImages);

        for(int i = 0; i < numberFramesInFlight; i++) {
            pro::cleanupVulkanBuffer(vkInitData, uboVertData[i]);
        }
        uboVertData.clear();

        for(int i = 0; i < allMeshes.size(); i++) {
            pro::cleanupVulkanMesh(vkInitData, allMeshes[i]);
        }
        allMeshes.clear();
        waitingMeshes.clear();
        readyToRenderMeshes.clear();
        copiesToCheck.clear();

        pro::cleanupVulkanPipeline(vkInitData, pipelineData);
        vkInitData.device().destroyQueryPool(queryPool);
        pro::cleanupFrameCommandData(vkInitData, commandData);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}