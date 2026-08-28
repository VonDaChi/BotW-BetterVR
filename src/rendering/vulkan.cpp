#include "vulkan.h"
#include "hooking/layer.h"
#include "instance.h"
#include "utils/logger.h"

RND_Vulkan::RND_Vulkan(VkInstance vkInstance, VkPhysicalDevice vkPhysDevice, VkDevice vkDevice): m_instance(vkInstance), m_physicalDevice(vkPhysDevice), m_device(vkDevice) {
    m_instanceDispatch = vkroots::tables::InstanceDispatches.find(vkInstance);
    m_physicalDeviceDispatch = vkroots::tables::PhysicalDeviceDispatches.find(vkPhysDevice);
    m_deviceDispatch = vkroots::tables::DeviceDispatches.find(vkDevice);

    // AMD GPU FIX: Initialize sType before calling vkGetPhysicalDeviceMemoryProperties2
    m_memoryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    m_memoryProperties.pNext = nullptr;
    m_physicalDeviceDispatch->GetPhysicalDeviceMemoryProperties2KHR(vkPhysDevice, &m_memoryProperties);

    VkPhysicalDeviceProperties props{};
    m_instanceDispatch->GetPhysicalDeviceProperties(vkPhysDevice, &props);

    uint64_t localVramBytes = 0;
    for (uint32_t i = 0; i < m_memoryProperties.memoryProperties.memoryHeapCount; ++i) {
        if ((m_memoryProperties.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
            localVramBytes += m_memoryProperties.memoryProperties.memoryHeaps[i].size;
        }
    }

    Log::print<INFO>("GPU: {} (vendor={:#06x}, device={:#06x}, driver={})", props.deviceName, props.vendorID, props.deviceID, props.driverVersion);
    if (localVramBytes > 0) {
        Log::print<INFO>("GPU VRAM (device local): {:.2f} GiB", double(localVramBytes) / (1024.0 * 1024.0 * 1024.0));
    }
}

RND_Vulkan::~RND_Vulkan() {
}

uint32_t RND_Vulkan::FindMemoryType(uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requirementsMask) {
    // AMD GPU FIX: Use actual memoryTypeCount instead of VK_MAX_MEMORY_TYPES to avoid reading uninitialized data
    const uint32_t memoryTypeCount = m_memoryProperties.memoryProperties.memoryTypeCount;
    for (uint32_t i = 0; i < memoryTypeCount; i++) {
        const uint32_t memoryTypeBits = (1u << i);
        const bool isRequiredMemoryType = (memoryTypeBitsRequirement & memoryTypeBits) != 0;
        const bool satisfiesFlags = (m_memoryProperties.memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask;

        if (isRequiredMemoryType && satisfiesFlags) {
            return i;
        }
    }
    checkAssert(false, "Failed to find suitable memory type");
    return 0;
}


static std::optional<VkPresentModeKHR> PickUnthrottledPresentMode(const vkroots::VkDeviceDispatch& pDispatch, VkSurfaceKHR surface) {
    if (surface == VK_NULL_HANDLE) {
        return std::nullopt;
    }

    uint32_t presentModeCount = 0;
    if (pDispatch.GetPhysicalDeviceSurfacePresentModesKHR(pDispatch.PhysicalDevice, surface, &presentModeCount, nullptr) != VK_SUCCESS || presentModeCount == 0) {
        return std::nullopt;
    }

    std::vector<VkPresentModeKHR> supportedPresentModes(presentModeCount);
    if (pDispatch.GetPhysicalDeviceSurfacePresentModesKHR(pDispatch.PhysicalDevice, surface, &presentModeCount, supportedPresentModes.data()) != VK_SUCCESS) {
        return std::nullopt;
    }
    supportedPresentModes.resize(presentModeCount);

    for (VkPresentModeKHR wantedMode : { VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR }) {
        if (std::ranges::find(supportedPresentModes, wantedMode) != supportedPresentModes.end()) {
            return wantedMode;
        }
    }
    return std::nullopt;
}

VkResult VRLayer::VkDeviceOverrides::CreateSwapchainKHR(const vkroots::VkDeviceDispatch& pDispatch, VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    std::optional<VkPresentModeKHR> unthrottledMode = PickUnthrottledPresentMode(pDispatch, pCreateInfo->surface);
    if (!unthrottledMode.has_value() || unthrottledMode.value() == pCreateInfo->presentMode) {
        return pDispatch.CreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    }

    Log::print<RENDERING>("Replacing Cemu swapchain present mode {} with {} so presenting the mirror window never blocks the thread that drives OpenXR", (uint32_t)pCreateInfo->presentMode, (uint32_t)unthrottledMode.value());

    VkSwapchainCreateInfoKHR createInfo = *pCreateInfo;
    createInfo.presentMode = unthrottledMode.value();
    return pDispatch.CreateSwapchainKHR(device, &createInfo, pAllocator, pSwapchain);
}