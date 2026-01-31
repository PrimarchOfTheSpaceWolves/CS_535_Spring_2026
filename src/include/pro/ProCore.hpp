#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstddef>
#include <functional>
#include <thread>
#include <chrono>
#include <atomic>
using namespace std;

// If uncommented, use dynamic dispatcher
//#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#define VULKAN_HPP_NO_NODISCARD_WARNINGS
//#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <vulkan/vk_enum_string_helper.h>
#include "VkBootstrap.h"

#define GLM_FORCE_CTOR_INIT
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"
#include "glm/gtx/string_cast.hpp"
#include "glm/gtc/type_ptr.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "vk_mem_alloc.h"

#include "stb_image.h"
#include "stb_image_write.h"

namespace pro {

    ///////////////////////////////////////////////////////////////////////////
    // Helper functions
    ///////////////////////////////////////////////////////////////////////////

    inline string get_full_error_string(string origin, string errorType, string msg) {
        return "[" + origin + "][" + errorType + "] " + msg;   
    };

    inline void print_error(string origin, string msg) {
        string full_error_msg = get_full_error_string(origin, "ERROR", msg);
        cerr << full_error_msg << endl;        
    };

    inline void print_warning(string origin, string msg) {
        string full_error_msg = get_full_error_string(origin, "WARNING", msg);
        cerr << full_error_msg << endl;        
    };

    inline void print_failure(string origin, string msg) {
        string full_error_msg = get_full_error_string(origin, "FAILURE", msg);
        cerr << full_error_msg << endl;        
    };

    inline void print_and_throw_error(string origin, string msg) {
        string full_error_msg = get_full_error_string(origin, "ERROR", msg);
        cerr << full_error_msg << endl;
        throw runtime_error(full_error_msg);
    };   

}
