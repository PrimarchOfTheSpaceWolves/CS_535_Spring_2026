#include <iostream>
#include <string>
#include "pro/Prometheus.hpp"

using namespace std;

///////////////////////////////////////////////////////////////////////////////
// STRUCTS
///////////////////////////////////////////////////////////////////////////////

struct ProVertex {
    glm::vec3 pos;
    glm::vec4 color;
};

///////////////////////////////////////////////////////////////////////////////
// GLOBALS
///////////////////////////////////////////////////////////////////////////////

bool didWindowResize = false;

///////////////////////////////////////////////////////////////////////////////
// GLFW CALLBACKS
///////////////////////////////////////////////////////////////////////////////

// Note: static prevents name conflicts in other cpp files

// When the window resizes/minimizes...
static void window_resize_callback(GLFWwindow* window, int width, int height) {
    didWindowResize = true;    
}

// When key events occur...
static void key_callback(   GLFWwindow *window,
                            int key,
                            int scancode,
                            int action,
                            int mods) {

    if(action == GLFW_PRESS || action == GLFW_REPEAT) {
        if(key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, true);
        }
    }
}

// When the mouse moves...
static void mouse_position_callback(GLFWwindow* window, double xpos, double ypos) {
    glm::vec2 curMouse = glm::vec2(xpos, ypos);
    //cout << "Mouse pos: " << glm::to_string(curMouse) << endl;
}

// When a mouse button is pressed/released/clicked...
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        cout << "Left mouse press." << endl;
    }
}

///////////////////////////////////////////////////////////////////////////////
// PER-FRAME "DRAWING" FUNCTION
///////////////////////////////////////////////////////////////////////////////

void recordFrame(   pro::VulkanInitData &vkInitData, 
                    pro::FrameCommandData &cd,
                    const pro::VulkanSwapImage &swapImage,
                    const pro::VulkanImage &depthImage,
                    pro::VulkanPipelineData &pipelineData,
                    vector<pro::VulkanMesh> allMeshes) {

    // Reset our command pool so it's cleared and ready to go
    vkInitData.device().resetCommandPool(cd.commandPool);
     
    // Begin recording
    cd.commandBuffer.begin(vk::CommandBufferBeginInfo());

    // Transition swap image from undefined to color buffer
    performVulkanImageTransition(cd.commandBuffer, swapImage.image, pro::IMAGE_TRANSITION_TYPE::UNDEF_TO_COLOR);

    // Define behavior for the color attachment (including clear color)
    vk::RenderingAttachmentInfoKHR colorAtt = pro::createColorAttachment(
        swapImage.view, 
        vk::ClearColorValue {0.0f, 1.0f, 1.0f, 1.0f});

    // Define behavior for the depth attachment
    vk::RenderingAttachmentInfoKHR depthAtt = pro::createDepthAttachment(depthImage.view);
        
    // Set rendering info and begin (dynamic) rendering
    vk::RenderingInfoKHR ri{};
    ri.setRenderArea(vk::Rect2D{ {0,0}, vkInitData.swapchain().extent })
        .setLayerCount(1)
        .setColorAttachments(colorAtt)
        .setPDepthAttachment(&depthAtt);

    cd.commandBuffer.beginRendering(ri);
    
    // Bind pipeline
    cd.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineData.pipeline);
    
    // Set up viewport and scissors
    vk::Viewport viewports[] = { pro::makeDefaultViewport(vkInitData) };    
    cd.commandBuffer.setViewport(0, viewports);
    
    vk::Rect2D scissors[] = { pro::makeDefaultScissors(vkInitData) };
    cd.commandBuffer.setScissor(0, scissors);

    // FOR NOW, just render all meshes
    for(auto &mesh : allMeshes) {
        pro::recordDrawVulkanMesh(cd.commandBuffer, mesh);
    }
    
    // End rendering
    cd.commandBuffer.endRendering();
   
    // Transition swap image from color buffer to presentation
    pro::performVulkanImageTransition(cd.commandBuffer, swapImage.image, pro::IMAGE_TRANSITION_TYPE::COLOR_TO_PRESENT);
    

    // End recording
    cd.commandBuffer.end();
}   

///////////////////////////////////////////////////////////////////////////////
// MAIN FUNCTION
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {
    cout << "BEGIN PROGRAM..." << endl;

    // Set app name
    string appName = "VulkanStart";
    string windowName = appName + ": realemj";

    ///////////////////////////////////////////////////////////////////////
    // GLFW
    ///////////////////////////////////////////////////////////////////////

    // Initialize GLFW
    if(!glfwInit()) {
        cerr << "ERROR: Cannot start GLFW!" << endl;
        exit(1);
    }
    
    // Create GLFW window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, true);
    GLFWwindow *window = glfwCreateWindow(800, 600, windowName.c_str(), nullptr, nullptr);

    // Was window successfully created?
    if(!window) {
        cerr << "ERROR: Cannot create GLFW window!" << endl;
        glfwTerminate();
        exit(1);
    }

    // Define GLFW callback functions
    glfwSetFramebufferSizeCallback(window, window_resize_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    
    // Create scope for Vulkan Init Data (to ensure proper cleanup)
    {
        ///////////////////////////////////////////////////////////////////////
        // VULKAN INIT DATA
        ///////////////////////////////////////////////////////////////////////

        // Creation information for basic Vulkan components
        pro::VulkanInitCreateInfo createInfo {};
        createInfo.appName = appName;
        // If you encounter errors with instance creation, try requesting Vulkan 1.3:
        // createInfo.requestedAppVulkanVersionMinor = 3;
        
        // If you encounter errors with compute and/or transfer queue creation, try these:
        createInfo.requireComputeQueue = false;
        createInfo.requireTransferQueue = false;

        createInfo.createSurfaceFunc = [window](VkInstance instance, VkSurfaceKHR& surface) {            
            return glfwCreateWindowSurface(instance, window, nullptr, &surface);
        };

        createInfo.getCurrentWindowSizeFunc = [window](int &width, int &height) {
            glfwGetFramebufferSize(window, &width, &height);
        };
    
        // Create the basic Vulkan components
        pro::VulkanInitData vkInitData(createInfo);
        
        cout << "** Chosen Physical Device: *********" << endl;        
        pro::printPhysicalDeviceProperties(vkInitData.physicalDevice());
        vkInitData.printQueues();

        // Create depth image(s)
        vector<pro::VulkanImage> allDepthImages {};
        int numberOfFramesInFlight = 1;
        pro::recreateAllVulkanDepthImages(vkInitData, allDepthImages, numberOfFramesInFlight);
        
        // Define resize function
        pro::OnResizeFunc resizeFunc = [&vkInitData, window, &allDepthImages, numberOfFramesInFlight]() {            
            int width = 0;
            int height = 0;

            do {
                glfwGetFramebufferSize(window, &width, &height);
                // If minimized, this will be 0,0. Block until restored.
                glfwWaitEvents(); // Actually waits/sleeps/blocks until an event happens
            } while (width == 0 || height == 0);
        
            // Safe to recreate
            vkInitData.recreateVulkanSwapchain();
            recreateAllVulkanDepthImages(vkInitData, allDepthImages, numberOfFramesInFlight);

            cout << "Swapchain recreated..." << endl;
        };

        ///////////////////////////////////////////////////////////////////////
        // VULKAN COMMAND DATA
        ///////////////////////////////////////////////////////////////////////

        // Create command data
        pro::FrameCommandData commandData = pro::createFrameCommandData(vkInitData);

        ///////////////////////////////////////////////////////////////////////
        // VULKAN GRAPHICS PIPELINE
        ///////////////////////////////////////////////////////////////////////

        // Set up creation info for pipeline
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

        // Set up vertex information
        pipelineCreateInfo.bindDesc = vk::VertexInputBindingDescription(
            0, sizeof(ProVertex), vk::VertexInputRate::eVertex);

        // POSITION
        pipelineCreateInfo.attribDesc.push_back(vk::VertexInputAttributeDescription(
            0, // location
            0, // binding
            vk::Format::eR32G32B32Sfloat,  // format
            offsetof(ProVertex, pos) // offset
        ));
        
        // COLOR
        pipelineCreateInfo.attribDesc.push_back(vk::VertexInputAttributeDescription(
            1, // location
            0, // binding
            vk::Format::eR32G32B32A32Sfloat,  // format
            offsetof(ProVertex, color) // offset
        ));

        // Actually create the pipeline data
        pro::VulkanPipelineData pipelineData = createVulkanPipeline(vkInitData, pipelineCreateInfo);

        ///////////////////////////////////////////////////////////////////////
        // MESH CREATION
        ///////////////////////////////////////////////////////////////////////

        // Create host data  
        vector<pro::HostMesh<ProVertex>> allHostMeshes {};
        
        pro::HostMesh<ProVertex> simpleQuad {};
        simpleQuad.vertices = {
            {{-0.5f, -0.5f, 0.5f},  {1,0,0,1}},
            {{0.5f, -0.5f, 0.5f},   {0,1,0,1}},
            {{0.5f, 0.5f, 0.5f},    {0,0,1,1}},
            {{-0.5f, 0.5f, 0.5f},   {1,1,1,1}}
        };
        simpleQuad.indices = { 0, 1, 2, 0, 2, 3 };        
        allHostMeshes.push_back(simpleQuad);

        // Create the Vulkan meshes
        vector<pro::VulkanMesh> allMeshes {};    
        allMeshes.resize(allHostMeshes.size());       
        for(unsigned int i = 0; i < allMeshes.size(); i++) {
            allMeshes[i] = pro::createVulkanMesh(vkInitData, simpleQuad, false);
            pro::copyToHostVisibleVulkanMesh(vkInitData, allMeshes[i], allHostMeshes[i]);            
        }

        ///////////////////////////////////////////////////////////////////////
        // MAIN RENDER LOOP
        ///////////////////////////////////////////////////////////////////////
        
        // While the window is still open...
        while (!glfwWindowShouldClose(window)) { 
            // Check for window/keyboard/mouse events...	
            glfwPollEvents();	

            // Did the window resize?
            if(didWindowResize) {
                didWindowResize = false;
                resizeFunc();
            }

            // Set frame-in-flight index (only one for now)
            unsigned int indexFlight = 0;

            // Acquire swap image
            unsigned int indexSwap = pro::acquireNextSwapImage(vkInitData, commandData, resizeFunc);

            // Record a frame
            recordFrame(
                vkInitData, 
                commandData, 
                vkInitData.swapchain().swaps[indexSwap], 
                allDepthImages[indexFlight],
                pipelineData,
                allMeshes);
                    
            // Submit to queue
            pro::submitToGraphicsQueue(vkInitData, commandData, indexSwap, resizeFunc);

            // Present
            if(!pro::presentSwapImage(vkInitData, commandData, indexSwap, resizeFunc)) {
                cout << "Warning: Presentation was not successful." << endl;
            }
        }
        
        ///////////////////////////////////////////////////////////////////////
        // CLEANUP
        ///////////////////////////////////////////////////////////////////////

        // Wait until device is completely idle
        vkInitData.device().waitIdle();

        // Cleanup assets
        for(auto &mesh : allMeshes) {
            pro::cleanupVulkanMesh(vkInitData, mesh);
        }
        allMeshes.clear();
        
        // Cleanup Vulkan-related stuff
        cleanupVulkanPipeline(vkInitData, pipelineData);
        cleanupFrameCommandData(vkInitData, commandData);
        cleanupAllVulkanDepthImages(vkInitData, allDepthImages);

        // VulkanInitData will be cleaned up automatically when it falls out of scope.
    }
    
    ///////////////////////////////////////////////////////////////////////
    // GLFW CLEANUP
    ///////////////////////////////////////////////////////////////////////

    glfwDestroyWindow(window);
    glfwTerminate();   

    // End program successfully
    return 0;
}
