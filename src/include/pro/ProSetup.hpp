#pragma once
#include "ProCore.hpp"

namespace pro { 

    ///////////////////////////////////////////////////////////////////////////
    // FUNCTION POINTERS 
    ///////////////////////////////////////////////////////////////////////////

    using CreateSurfaceFunc = std::function<VkResult(VkInstance, VkSurfaceKHR&)>;
    
    ///////////////////////////////////////////////////////////////////////////
    // STRUCTS 
    ///////////////////////////////////////////////////////////////////////////

    struct VulkanQueue {
        vk::Queue queue {};
        unsigned int index = 0;
    };

    struct VulkanSwapImage {
        vk::Image image {};
        vk::ImageView view {};
        vk::Semaphore renderDone {};
    };

    struct VulkanSwapChain {
        vk::SwapchainKHR chain {};
        vector<VulkanSwapImage> swaps {};        
        vk::Extent2D extent {};
        vk::Format format {};
    };

    struct VulkanInitCreateInfo {
        // Vulkan instance
        string appName = "ProApp";
        string engineName = "ProEngine";
        int requestedAppVulkanVersionMajor = 1;
        int requestedAppVulkanVersionMinor = 4;

        // Vulkan physical device
        vk::PhysicalDeviceFeatures reqFeaturesBase {};
        vk::PhysicalDeviceVulkan12Features reqFeatures12 {};
        vk::PhysicalDeviceVulkan13Features reqFeatures13 {};   
        vector<string> reqExtensions {};

        // Surface
        CreateSurfaceFunc createSurfaceFunc = nullptr;

        // Swapchain
        VkSurfaceFormatKHR desiredSwapchainFormat {};
            
        VulkanInitCreateInfo() {
            // Set default requested features
            reqFeaturesBase.samplerAnisotropy = true;
            
            reqFeatures13.dynamicRendering = true;
            reqFeatures13.synchronization2 = true;

            // Make sure it stores values in linear space, BUT
            // does gamma correction during presentation            
            desiredSwapchainFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
            desiredSwapchainFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        };
    };
        
    ///////////////////////////////////////////////////////////////////////////
    // Helper functions
    ///////////////////////////////////////////////////////////////////////////

    inline const char* getDeviceTypeString(vk::PhysicalDeviceType t) {
        switch (t) {
            case vk::PhysicalDeviceType::eIntegratedGpu: return "Integrated GPU";
            case vk::PhysicalDeviceType::eDiscreteGpu:   return "Discrete GPU";
            case vk::PhysicalDeviceType::eVirtualGpu:    return "Virtual GPU";
            case vk::PhysicalDeviceType::eCpu:           return "CPU";
            default:                                     return "Other";
        }
    };

    inline void printPhysicalDeviceProperties(const vk::PhysicalDevice &pd) {
        vk::PhysicalDeviceProperties props = pd.getProperties();
        uint32_t ver = props.apiVersion;
        
        cout << "Name: " << props.deviceName.data() << endl;
        cout << "Type: " << getDeviceTypeString(props.deviceType) << endl;    
        cout << "API Version: " 
            << VK_VERSION_MAJOR(ver) << "." 
            << VK_VERSION_MINOR(ver) << "." 
            << VK_VERSION_PATCH(ver) << endl;
    };

    inline void listAvailablePhysicalDevices(const vk::Instance &instance) {
        vector<vk::PhysicalDevice> phys = instance.enumeratePhysicalDevices();
        
        cout << "Found " << phys.size() << " physical device(s):" << endl;
        for (int i = 0; i < phys.size(); i++) {
            cout << "** Device " << i << " ***********" << endl;
            printPhysicalDeviceProperties(phys[i]);        
        }
    };

    inline void printMaxSupportedVulkanVersion() {        
        uint32_t apiVer = vk::enumerateInstanceVersion();
        cout << "Loader supports up to Vulkan "
                << VK_VERSION_MAJOR(apiVer) << "."
                << VK_VERSION_MINOR(apiVer) << "."
                << VK_VERSION_PATCH(apiVer) << endl;
    }

    inline bool getVulkanQueue( vkb::Device vkbDevice, 
                                vkb::QueueType queueType, 
                                VulkanQueue &queueData) {

        bool useDedicated = true;

        // Try to get dedicated queue first
        auto queueRet = vkbDevice.get_dedicated_queue(queueType);
        if(!queueRet) {
            useDedicated = false;
        }

        // Get the desired queue (if dedicated check failed)
        if(!useDedicated) {
            queueRet = vkbDevice.get_queue(queueType);
            if(!queueRet) {
                cerr << "Error: " << queueRet.error().message() << endl;
                return false;
            } 
        }
        
        // Convert to vk::Queue
        queueData.queue = vk::Queue { queueRet.value() };

        // Get queue index
        if(!useDedicated) {
            queueData.index = vkbDevice.get_queue_index(queueType).value();
        }
        else {
            queueData.index = vkbDevice.get_dedicated_queue_index(queueType).value();
        }

        // Success!
        return true;
    };
    
    ///////////////////////////////////////////////////////////////////////////
    // CLASSES 
    ///////////////////////////////////////////////////////////////////////////
        
    class VulkanInitData {
    public:
        VulkanInitData(VulkanInitCreateInfo &createInfo) {
            // Quick sanity check...is the surface creation function defined?
            if(!createInfo.createSurfaceFunc) {
                throw runtime_error("createSurfaceFunc cannot be null!");
            }

            // Instance
            vkb::InstanceBuilder builder;        
            auto instRet = builder.set_app_name(createInfo.appName.c_str())
                                .set_engine_name(createInfo.engineName.c_str())
                                .request_validation_layers()
                                .use_default_debug_messenger()
                                .require_api_version(
                                    createInfo.requestedAppVulkanVersionMajor,
                                    createInfo.requestedAppVulkanVersionMinor,
                                    0)
                                .build();
            if(!instRet) {
                throw runtime_error(instRet.error().message());               
            }

            vkb::Instance vkbInstance = instRet.value();
            this->bootInstance_ = vkbInstance;
            this->instance_ = vk::Instance { vkbInstance.instance };

            // Surface
            VkSurfaceKHR surface = nullptr;
            VkResult surfErr = createInfo.createSurfaceFunc(vkbInstance.instance, surface);
            if(surfErr != VK_SUCCESS) {                 
                vkb::destroy_instance(bootInstance_);                       
                throw runtime_error(surfErr + "");
            }
            surface_ = vk::SurfaceKHR { surface };

            // Physical device           
            vkb::PhysicalDeviceSelector selector { vkbInstance };  
            selector.set_surface(surface);      
            selector.set_minimum_version(
                createInfo.requestedAppVulkanVersionMajor,
                createInfo.requestedAppVulkanVersionMinor);                 

            for(auto reqExt : createInfo.reqExtensions) {
                selector.add_required_extension(reqExt.c_str());
            }

            selector.set_required_features(createInfo.reqFeaturesBase);      
            selector.set_required_features_12(createInfo.reqFeatures12);                       
            selector.set_required_features_13(createInfo.reqFeatures13);   

            auto physRet = selector.select();

            if(!physRet) {  
                instance_.destroySurfaceKHR(surface_); 
                vkb::destroy_instance(bootInstance_);                       
                throw runtime_error(physRet.error().message());                
            }

            vkb::PhysicalDevice vkbPhysicalDevice = physRet.value();
            physicalDevice_ = vk::PhysicalDevice { vkbPhysicalDevice.physical_device };

            // Logical device        
            vkb::DeviceBuilder deviceBuilder { vkbPhysicalDevice };
            auto devRet = deviceBuilder.build();
            if(!devRet) {
                instance_.destroySurfaceKHR(surface_); 
                vkb::destroy_instance(bootInstance_);                 
                throw runtime_error(devRet.error().message());
            }
            vkb::Device vkbDevice = devRet.value();
            device_ = vk::Device { vkbDevice.device };
            bootDevice_ = vkbDevice;

            // Do we want a dynamic dispatcher?
            #if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
                // 1) Get function pointer for "vkGetInstanceProcAddr", the function used to find ALL other functions.
                vk::detail::DynamicLoader dl;
                PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = 
                    dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");

                // 2) Initialize Instance-level functions                
                VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    
                // 3) Initialize specific Instance functions                
                VULKAN_HPP_DEFAULT_DISPATCHER.init(instance_);

                // D. Initialize Device-level functions                
                VULKAN_HPP_DEFAULT_DISPATCHER.init(device_);
            #endif

            // Get queues
            bool graphQueueSuccess = getVulkanQueue(vkbDevice, vkb::QueueType::graphics, graphicsQueue_);
            bool presentQueueSuccess = getVulkanQueue(vkbDevice, vkb::QueueType::present, presentQueue_);
            bool computeQueueSuccess = getVulkanQueue(vkbDevice, vkb::QueueType::compute, computeQueue_);
            bool transferQueueSuccess = getVulkanQueue(vkbDevice, vkb::QueueType::transfer, transferQueue_);

            if(!graphQueueSuccess || !presentQueueSuccess || !computeQueueSuccess || !transferQueueSuccess) {
                device_.destroy();
                instance_.destroySurfaceKHR(surface_); 
                vkb::destroy_instance(bootInstance_);                 
                throw runtime_error("Could not retrieve requested queues!");
            }

            // Create swapchain
            swapchain_create_format_ = createInfo.desiredSwapchainFormat;
            if(!createVulkanSwapchain()) {
                device_.destroy();
                instance_.destroySurfaceKHR(surface_); 
                vkb::destroy_instance(bootInstance_);                 
                throw runtime_error("Unable to create swapchain!");
            }   
            
            
            // VMA
            VmaAllocatorCreateInfo allocatorInfo{};
            allocatorInfo.instance = vkbInstance.instance;
            allocatorInfo.physicalDevice = vkbPhysicalDevice.physical_device;
            allocatorInfo.device = vkbDevice.device;

            auto vmaResult = vmaCreateAllocator(&allocatorInfo, &(allocator_));
            if(!devRet) {    
                cleanupVulkanSwapchain();            
                device_.destroy();
                instance_.destroySurfaceKHR(surface_); 
                vkb::destroy_instance(bootInstance_);                 
                throw runtime_error(devRet.error().message());
            }            
        };
        
        ~VulkanInitData() {
            device_.waitIdle();
            vmaDestroyAllocator(allocator_);
            cleanupVulkanSwapchain();
            device_.destroy();
            instance_.destroySurfaceKHR(surface_); 
            vkb::destroy_instance(bootInstance_);             
        };

        // Copy: forbidden (unique ownership)
        VulkanInitData(const VulkanInitData&)            = delete;
        VulkanInitData& operator=(const VulkanInitData&) = delete;

        // Move: allowed
        VulkanInitData(VulkanInitData&&) noexcept            = default;
        VulkanInitData& operator=(VulkanInitData&&) noexcept = default;

        // Getters        
        const vk::Instance& instance() const noexcept { return instance_; };
        const vk::PhysicalDevice& physicalDevice() const noexcept { return physicalDevice_; };
        const vk::Device& device() const noexcept { return device_; };
        const VulkanQueue& graphicsQueue() const noexcept { return graphicsQueue_; };
        const VulkanQueue& presentQueue() const noexcept {  return presentQueue_; };
        const VulkanQueue& computeQueue() const noexcept {  return computeQueue_; };
        const VulkanQueue& transferQueue() const noexcept {  return transferQueue_; };
        const VulkanSwapChain& swapchain() const noexcept { return swapchain_; };
        const VmaAllocator allocator() const noexcept { return allocator_; };

        // Other member functions        
        void recreateVulkanSwapchain() {    
            // Wait until device is idle
            device_.waitIdle();

            // Cleanup swapchain data
            cleanupVulkanSwapchain();
            
            // (Re)create swap chain and image views
            createVulkanSwapchain();
        };

        void printQueues(std::ostream& os = std::cout) {
            os << "** QUEUES: ***************" << endl;
            os << "Graphics: " << graphicsQueue_.index << endl;
            os << "Present: " << presentQueue_.index << endl;
            os << "Compute: " << computeQueue_.index << endl;
            os << "Transfer: " << transferQueue_.index << endl;
            os << endl;

            if(isComputeDedicated()) {
                os << "Using dedicated compute queue." << endl;
            }
            else {
                os << "Compute queue same as graphics queue." << endl;
            }

            if(isTransferDedicated()) {
                os << "Using dedicated transfer queue." << endl;
            }
            else {
                os << "Transfer queue same as graphics queue." << endl;
            }
            
            os << "**************************" << endl;            
        };

        bool isComputeDedicated() {
            return (graphicsQueue_.queue != computeQueue_.queue);
        };

        bool isTransferDedicated() {
            return (graphicsQueue_.queue != transferQueue_.queue);
        };

    private:         
        vkb::Instance bootInstance_ {};          // Clean up explicitly
        vk::Instance instance_ {};               // Do NOT clean up explicitly  
        
        vk::SurfaceKHR surface_ {};              // Clean up explicitly
        
        vk::PhysicalDevice physicalDevice_ {};   // No NEED to clean up

        vkb::Device bootDevice_ {};              // Do NOT clean up explicitly
        vk::Device device_ {};                   // Cleaned up explicitly

        VulkanQueue graphicsQueue_ {};           // No NEED to clean up
        VulkanQueue presentQueue_ {};            // No NEED to clean up
        VulkanQueue computeQueue_ {};            // No NEED to clean up
        VulkanQueue transferQueue_ {};           // No NEED to clean up

        VulkanSwapChain swapchain_ {};           // Cleaned up explicitly
        VkSurfaceFormatKHR swapchain_create_format_ {};   // No NEED to clean up

        VmaAllocator allocator_ {};              // Cleaned up explicitly              

        bool createVulkanSwapchain() {
            // Create swapchain
            vkb::SwapchainBuilder swapchainBuilder { bootDevice_ };
            auto swapRet = swapchainBuilder.set_desired_format(swapchain_create_format_).build();

            if(!swapRet) {
                cerr << "initVulkanBootstrap: Failed to create swapchain." << endl;
                cerr << "Error: " << swapRet.error().message() << endl;
                return false;
            }
            
            vkb::Swapchain vkSwapchain = swapRet.value();

            // Convert to our data structure so we use VulkanHpp consistently
            swapchain_.chain = vk::SwapchainKHR { vkSwapchain.swapchain };
            swapchain_.format = vk::Format(vkSwapchain.image_format);
            swapchain_.extent = vk::Extent2D { vkSwapchain.extent };
            
            vector<VkImageView> vkViews = vkSwapchain.get_image_views().value();
            vector<VkImage> vkImages = vkSwapchain.get_images().value();

            for(unsigned int i = 0; i < vkViews.size(); i++) {
                VulkanSwapImage vimage {};
                vimage.image = vk::Image { vkImages.at(i) };
                vimage.view = vk::ImageView { vkViews.at(i) };
                vimage.renderDone = device_.createSemaphore(vk::SemaphoreCreateInfo());
                swapchain_.swaps.push_back(vimage);
            }

            return true;
        };

        void cleanupVulkanSwapchain() {        
            for(unsigned int i = 0; i < swapchain_.swaps.size(); i++) {
                device_.destroyImageView(swapchain_.swaps[i].view);    
                device_.destroySemaphore(swapchain_.swaps[i].renderDone);        
            }       
            swapchain_.swaps.clear();             
            device_.destroySwapchainKHR(swapchain_.chain);
            swapchain_ = {};
        };     

    };
}
