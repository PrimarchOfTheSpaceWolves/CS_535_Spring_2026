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

    string appName = "ProfExercises05";
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
        
        
        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if(didWindowResize) {
                resizeFunc();
                didWindowResize = false;
            }

        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}