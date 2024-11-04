#pragma once
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "win32App.h"
#include "timer.h"

class VkApp : public Win32App
{
public:
    VkApp(const Entry& entry, LPCTSTR caption, uint32_t width, uint32_t height);
    ~VkApp();
    void close() override;
    void onIdle() override;
    void onPaint() override;

private:
    void createInstance();
    void createPhysicalDevice();
    void createLogicalDevice();
    void createWin32Surface();
    void createSwapchain();
    void createRenderPass();
    void createFramebuffer();
    void createCommandPools();
    void createCommandBuffers();
    void createSyncPrimitices();
    uint32_t aquireNextImage() const;
    void submit(uint32_t imageIndex);
    void present(uint32_t imageIndex);
    void waitForPresentComplete(uint32_t imageIndex);
    bool findExtension(const char *extensionName) const;
    uint32_t chooseFamilyIndex(VkQueueFlagBits queueType) const;

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT debugReportCallback = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkCommandPool graphicsCmdPool = VK_NULL_HANDLE;
    VkCommandPool computeCmdPool = VK_NULL_HANDLE;
    VkCommandPool transferCmdPool = VK_NULL_HANDLE;
    VkCommandBuffer computeCmdBuffer = VK_NULL_HANDLE;
    VkCommandBuffer transferCmdBuffer = VK_NULL_HANDLE;
    VkSemaphore presentSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;

    std::vector<VkExtensionProperties> extensionProperties;
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkCommandBuffer> cmdBuffers;
    std::vector<VkFence> cmdSubmitFences;

    Timer timer;
    float time = 0.f;
    uint32_t frameCount = 0;
    uint32_t fps = 0;
};
