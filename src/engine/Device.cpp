#define VMA_IMPLEMENTATION
#include "Device.h"

#include "Debugger.h"
#include "../jobs/Worker.h"

#include <vector>
#include <map>
#include <set>

using namespace vecs;

Device::Device(VkInstance instance, VkSurfaceKHR surface) {
    pickPhysicalDevice(instance, surface);
    createLogicalDevice();
    createMemoryAllocator(instance);
}

VkCommandPool Device::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags) {
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
    cmdPoolInfo.flags = createFlags;

    VkCommandPool commandPool;
    VK_CHECK_RESULT(vkCreateCommandPool(logical, &cmdPoolInfo, nullptr, &commandPool));

    return commandPool;
}

Buffer Device::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage allocUsage) {
    VkBufferCreateInfo vbInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vbInfo.size = size;
    vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage;
    vbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo info;
    info.flags = 0;
    info.requiredFlags = 0;
    info.preferredFlags = 0;
    info.memoryTypeBits = 0;
    info.usage = allocUsage;
    info.pool = VMA_NULL;

    Buffer buffer(allocator);
    buffer.size = size;
    VK_CHECK_RESULT(vmaCreateBuffer(allocator, &vbInfo, &info, &buffer.buffer, &buffer.allocation, nullptr));
    return buffer;
}

Buffer Device::createStagingBuffer(VkDeviceSize size) {
    VkBufferCreateInfo vbInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vbInfo.size = size;
    vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    vbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo info;
    info.flags = 0;
    info.requiredFlags = 0;
    info.preferredFlags = 0;
    info.memoryTypeBits = 0;
    info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    info.pool = VMA_NULL;

    Buffer buffer(allocator);
    buffer.size = size;
    VK_CHECK_RESULT(vmaCreateBuffer(allocator, &vbInfo, &info, &buffer.buffer, &buffer.allocation, nullptr));
    return buffer;
}

void Device::cleanupBuffer(Buffer buffer) {
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    buffer.buffer = VK_NULL_HANDLE;
}

void Device::copyBuffer(Buffer* src, Buffer* dest, Worker* worker, VkBufferCopy* copyRegion) {
    // Get a command buffer to use
    VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, worker->commandPool);

    // Determine how much to copy
    VkBufferCopy bufferCopy = {};
    if (copyRegion == nullptr)
        bufferCopy.size = src->size;
    else
        bufferCopy = *copyRegion;

    // Copy the buffers
    vkCmdCopyBuffer(copyCmd, src->buffer, dest->buffer, 1, &bufferCopy);

    // End our command buffer and submit it
    submitCommandBuffer(copyCmd, worker);
}

VkCommandBuffer Device::createCommandBuffer(VkCommandBufferLevel level, VkCommandPool commandPool, bool begin) {
    // Tell it which pool to create a command buffer for, with the given level
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = level;
    allocInfo.commandBufferCount = 1;

    // Create the buffer
    VkCommandBuffer buffer;
    VK_CHECK_RESULT(vkAllocateCommandBuffers(logical, &allocInfo, &buffer));

    if (begin) beginCommandBuffer(buffer);

    return buffer;
}

std::vector<VkCommandBuffer> Device::createCommandBuffers(VkCommandBufferLevel level, int amount, VkCommandPool commandPool, bool begin) {
    // Tell it which pool to create command buffers for, and how many, with the given level
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = level;
    allocInfo.commandBufferCount = amount;

    // Create the buffers
    std::vector<VkCommandBuffer> buffers(amount);
    VK_CHECK_RESULT(vkAllocateCommandBuffers(logical, &allocInfo, buffers.data()));

    if (begin)
        for (VkCommandBuffer buffer : buffers) beginCommandBuffer(buffer);

    return buffers;
}

void Device::submitCommandBuffer(VkCommandBuffer buffer, Worker* worker, bool free) {
    VK_CHECK_RESULT(vkEndCommandBuffer(buffer));

    // Submit our command buffer to its queue
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;

    if (worker->queueLock != nullptr) {
        worker->queueLock->lock();
    }
    vkQueueSubmit(worker->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(worker->graphicsQueue);
    if (worker->queueLock != nullptr) {
        worker->queueLock->unlock();
    }

    // Destroy our command buffer
    if (free) vkFreeCommandBuffers(logical, worker->commandPool, 1, &buffer);
}

uint32_t Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    // Get our different types of memories on our physical device
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physical, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        // Check if this type of memory supports our filter and has all the properties we need
        // TODO rank and choose best memory type (e.g. use VRAM before swap)
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    // Throw error if we can't find any memory that suits our needs
    throw std::runtime_error("failed to find suitable memory type!");
}

void Device::cleanup() {
    // Destroy our memory allocator
    vmaDestroyAllocator(allocator);

    // Destroy our logical device
    vkDestroyDevice(logical, nullptr);
}

void Device::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
    // Find how many physical devices we have
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    // Throw an error if we have no physical devices
    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    // Get each physical device
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Rate each physical device and insert each device and score into a map
    std::multimap<int, PhysicalDeviceCandidate> candidates;
    for (const auto& device : devices) {
        PhysicalDeviceCandidate candidate;
        candidate.physicalDevice = device;
        candidate.indices = findQueueFamilies(device, surface);
        candidate.swapChainSupport = querySwapChainSupport(device, surface);

        candidates.insert(std::make_pair(rateDeviceSuitability(candidate), candidate));
    }

    // Get the best scoring key-value pair (the map sorts on insert), and check if it has a non-zero score
    if (candidates.rbegin()->first > 0) {
        // Get the best score's device and indices (the second item in the key-value pair)
        PhysicalDeviceCandidate candidate = candidates.rbegin()->second;
        // Set the physical device
        physical = candidate.physicalDevice;
        queueFamilyIndices = candidate.indices;
        swapChainSupport = candidate.swapChainSupport;
        
        // Add device information to debug log
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physical, &deviceProperties);

        std::stringstream ss;
        ss << "Physical Device Information:\n";
        uint32_t major = deviceProperties.apiVersion >> 22;
        uint32_t minor = (deviceProperties.apiVersion >> 12) & 0x3ff;
        uint32_t patch = deviceProperties.apiVersion & 0xfff;
        ss << "\tAPI Version: " << major << "." << minor << "." << patch << "\n";
        ss << "\tDevice ID: 0x" << std::hex << deviceProperties.deviceID << std::dec << "\n";
        ss << "\tDevice Name: " << deviceProperties.deviceName << "\n";
        std::string deviceType;
        switch (deviceProperties.deviceType) {
        case (0): deviceType = "PHYSICAL_DEVICE_TYPE_OTHER"; break;
        case (1): deviceType = "PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU"; break;
        case (2): deviceType = "PHYSICAL_DEVICE_TYPE_DISCRETE_GPU"; break;
        case (3): deviceType = "PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU"; break;
        case (4): deviceType = "PHYSICAL_DEVICE_TYPE_CPU"; break;
        default: deviceType = "UNKNOWN_VkPhysicalDeviceType"; break;
        }
        ss << "\tDevice Type: " << deviceType << "\n";
        ss << "\tDriver Version: 0x" << std::hex << deviceProperties.driverVersion << std::dec << "\n";
        ss << "\tVendor ID: 0x" << std::hex << deviceProperties.vendorID << "\n";
        ss << "\tGraphics Queue Count: " << queueFamilyIndices.graphicsQueueCount;
        // TODO any more information?

        Debugger::addLog(DEBUG_LEVEL_VERBOSE, ss.str());
    } else {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    // Find out how many queue families we have
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    // Find the properties for all our queue families
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        // Check each queue family for one that supports VK_QUEUE_GRAPHICS_BIT,
        // selecting the one with the highest number of queues so we can have the
        // highest chance of having enough for our optimal number of worker threads
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && queueFamily.queueCount > indices.graphicsQueueCount) {
            indices.graphics = i;
            indices.graphicsQueueCount = queueFamily.queueCount;
        }

        // Check each queue family for one that supports presenting to the window system
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.present = i;
        }

        i++;
    }

    return indices;
}

SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    // Get the capabilities of our surface
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

    // Find how many surface formats the physical device supports
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    // If the number of formats supported is non-zero, add them to our details.formats list
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
    }

    // Find how many present modes the physical device supports
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    // If the number of modes is non-zero, add them to our details.presentModes list
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

int Device::rateDeviceSuitability(PhysicalDeviceCandidate candidate) {
    VkPhysicalDevice device = candidate.physicalDevice;
    QueueFamilyIndices indices = candidate.indices;
    SwapChainSupportDetails swapChainSupport = candidate.swapChainSupport;

    // Find the physical properties of this device
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    // Check its queue families and ensure it supports each feature we need
    // isComplete will check to make sure each feature we need exists at some index
    // We don't care what index, just that such an index exists
    // If it doesn't, return 0 to represent complete non-suitability
    if (!indices.isComplete())
        return 0;

    // Check if it has all the physical device extensions we need
    // Otherwise return 0 to represent complete non-suitability
    if (!checkDeviceExtensionSupport(device))
        return 0;

    // Check if the device supports the swap chains we need
    // And, as before, if it doesn't then return 0 to represent complete non-suitability
    if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty())
        return 0;

    // Find the physical features of the device
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    // If the device doesn't support anisotropy, its non-suitable
    if (!supportedFeatures.samplerAnisotropy)
        return 0;

    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        score += 1000;

    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;

    return score;
}

bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    // Find out how many extensions the device has
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    // Get the properties for each extension
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    // Create a set of all required extensions
    std::set<std::string> requiredExtensions(deviceExtensions, deviceExtensions + deviceExtensionsSize);

    // Go through each extension the device has. Attempt to remove its name from our
    // list of required components
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    // If all our required extensions got removed, then this device supports all of them
    return requiredExtensions.empty();
}

void Device::createLogicalDevice() {
    // Create information structs for each of our device queue families
    // Configure it as appropriate and give it our queue family indices
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    
    // We'll give them equal priority
    std::vector<float> queuePriorities;
    // 1 for the Renderer and 2 for worlds (which will flip flop between the active and loading world),
    // and 1 for each worker thread. Since our main thread shouldn't have to compete with the worker threads,
    // our preferred number of worker threads is thread::hardware_concurrency() - 1, and we're fine with the world loading
    // thread having to compete because it is relatively rare over the application's lifetime
    // Capped at max queue count in this family
    uint32_t desiredQueues = std::max(1u, std::thread::hardware_concurrency() - 1) + 3;
    for (int i = std::min(desiredQueues, queueFamilyIndices.graphicsQueueCount) - 1; i >= 0; i--)
        queuePriorities.push_back(1);

    // Present queue
    // If the queue families have the same index then the graphics queue
    // will also act as our present queue
    // (not vice versa, because we need more graphics queues than present queues)
    if (queueFamilyIndices.graphics != queueFamilyIndices.present) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndices.present.value();
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = queuePriorities.data();
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Graphics queue
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndices.graphics.value();
    queueCreateInfo.queueCount = static_cast<uint32_t>(queuePriorities.size());
    queueCreateInfo.pQueuePriorities = queuePriorities.data();
    queueCreateInfos.push_back(queueCreateInfo);

    // List the physical device features we need our logical device to support
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    // Create the information struct for our logical device
    // Configuring it with our device queue and required features
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;

    // Setup our device-specific extensions
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensionsSize);
    createInfo.ppEnabledExtensionNames = deviceExtensions;
    
    // Create the logical device and throw an error if anything goes wrong
    VK_CHECK_RESULT(vkCreateDevice(physical, &createInfo, nullptr, &logical));
}

void Device::createMemoryAllocator(VkInstance instance) {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physical;
    allocatorInfo.device = logical;
    allocatorInfo.instance = instance;

    VK_CHECK_RESULT(vmaCreateAllocator(&allocatorInfo, &allocator));
}

void Device::beginCommandBuffer(VkCommandBuffer buffer) {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // Being the command buffer
    VK_CHECK_RESULT(vkBeginCommandBuffer(buffer, &beginInfo));
}

void Device::beginSecondaryCommandBuffer(VkCommandBuffer buffer, VkCommandBufferInheritanceInfo* info) {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    beginInfo.pInheritanceInfo = info;

    // Being the command buffer
    VK_CHECK_RESULT(vkBeginCommandBuffer(buffer, &beginInfo));
}
