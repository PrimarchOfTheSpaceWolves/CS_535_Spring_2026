#include <iostream>
#include <string>
#include "pro/Prometheus.hpp"
using namespace std;

bool didWindowResize = false;

static void window_resize_callback(GLFWwindow* window, int width, int height) {
    didWindowResize = true;
}

int main(int argc, char **argv) {
    cout << "Starting exercises!" << endl;

    if(!glfwInit()) {
        cerr << "FAILED TO INIT GLFW!" << endl;
        exit(1);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, true);

    string appName = "ProfExercises06";
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

        pro::OnResizeFunc resizeFunc = [&vkInitData, window]() {
            int width = 0;
            int height = 0;
            do {
                glfwGetFramebufferSize(window, &width, &height);
                glfwWaitEvents();
            } while(width == 0 || height == 0);
            vkInitData.recreateVulkanSwapchain();
            cout << "Swapchain recreated..." << endl;
        };

        pro::FrameCommandData commandData = pro::createFrameCommandData(vkInitData);
        uint32_t framesRendered = 0;
        int numberFramesInFlight = 1;

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

            commandData.commandBuffer.resetQueryPool(queryPool, 0, 2);
            commandData.commandBuffer.writeTimestamp2(
                vk::PipelineStageFlagBits2::eTopOfPipe, queryPool, 0);

            pro::performVulkanImageTransition(
                commandData.commandBuffer,
                vkInitData.swapchain().swaps[indexSwap].image,
                pro::IMAGE_TRANSITION_TYPE::UNDEF_TO_COLOR
            );

            // TODO: This is where the rendering magic happens

            pro::performVulkanImageTransition(
                commandData.commandBuffer,
                vkInitData.swapchain().swaps[indexSwap].image,
                pro::IMAGE_TRANSITION_TYPE::COLOR_TO_PRESENT
            );

            commandData.commandBuffer.writeTimestamp2(
                vk::PipelineStageFlagBits2::eBottomOfPipe, queryPool, 1);

            commandData.commandBuffer.end();

            pro::submitToGraphicsQueue(vkInitData, commandData, indexSwap, resizeFunc);

            if(!pro::presentSwapImage(vkInitData, commandData, indexSwap, resizeFunc)) {
                cerr << "Error: Presentation not successful!" << endl;
            }

            framesRendered++;

            uint64_t timestamps[2] = {};
            vkInitData.device().getQueryPoolResults(
                queryPool, 0, 2, sizeof(timestamps), timestamps, 
                sizeof(uint64_t), 
                vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
             
            auto props = vkInitData.physicalDevice().getProperties();
            double nsPerTick = props.limits.timestampPeriod;
            double deltaNs = (timestamps[1] - timestamps[0])*nsPerTick;
            cout << "TIME: " << deltaNs << endl;
        }

        vkInitData.device().waitIdle();
        vkInitData.device().destroyQueryPool(queryPool);
        pro::cleanupFrameCommandData(vkInitData, commandData);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}