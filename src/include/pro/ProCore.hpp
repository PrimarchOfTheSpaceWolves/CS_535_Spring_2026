#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstddef>
#include <functional>
#include <thread>
#include <chrono>
using namespace std;

// If uncommented, use dynamic dispatcher
//#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#define VULKAN_HPP_NO_NODISCARD_WARNINGS
//#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
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
