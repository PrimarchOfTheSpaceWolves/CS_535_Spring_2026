#include <iostream>
#include <string>
#include "pro/Prometheus.hpp"
using namespace std;

int main(int argc, char **argv) {
    cout << "Starting exercises!" << endl;

    if(!glfwInit()) {
        cerr << "FAILED TO INIT GLFW!" << endl;
        exit(1);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, true);

    string appName = "ProfExercises03";
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
        
        
        // TODO

    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}