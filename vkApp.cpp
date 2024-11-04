#include <array>
#include <string>
#include <limits>
#include <cassert>
#include "vkApp.h"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define WAIT_PRESENT_FENCE

#define CHECK_SUCCEEDED(result, message)\
    if (VK_SUCCESS != result)\
        throw std::runtime_error(message)

VkApp::VkApp(const Entry& entry, LPCTSTR caption, uint32_t width, uint32_t height):
    Win32App(entry, caption, width, height)
{
    createInstance();
    createPhysicalDevice();
    createLogicalDevice();
    createWin32Surface();
    createSwapchain();
    createRenderPass();
    createFramebuffer();
    createCommandPools();
    createCommandBuffers();
    createSyncPrimitices();
    timer.run();
}

VkApp::~VkApp()
{
    vkDestroyFence(device, transferFence, nullptr);
    for (auto fence: cmdSubmitFences)
        vkDestroyFence(device, fence, nullptr);
    vkDestroySemaphore(device, presentSemaphore, nullptr);
    vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
    vkFreeCommandBuffers(device, transferCmdPool, 1, &transferCmdBuffer);
    vkFreeCommandBuffers(device, computeCmdPool, 1, &computeCmdBuffer);
    vkFreeCommandBuffers(device, graphicsCmdPool, (uint32_t)cmdBuffers.size(), cmdBuffers.data());
    vkDestroyCommandPool(device, transferCmdPool, nullptr);
    vkDestroyCommandPool(device, computeCmdPool, nullptr);
    vkDestroyCommandPool(device, graphicsCmdPool, nullptr);
    for (auto framebuffer: framebuffers)
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
    for (auto imageView: swapchainImageViews)
        vkDestroyImageView(device, imageView, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
#ifdef _DEBUG
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    if (vkDestroyDebugReportCallbackEXT)
        vkDestroyDebugReportCallbackEXT(instance, debugReportCallback, nullptr);
#endif // _DEBUG
    vkDestroyInstance(instance, nullptr);
}

void VkApp::close()
{
    Win32App::close();
}

void VkApp::onIdle()
{
    onPaint();
}

void VkApp::onPaint()
{
    constexpr uint64_t timeout = 10 * 1000000; // 10 ms
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, timeout, presentSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (VK_TIMEOUT == result)
        OutputDebugStringA("acquire image timeout has expired\n");
    else if (VK_SUCCESS != result)
        return;

    VkFramebuffer framebuffer = framebuffers[imageIndex];
    VkCommandBuffer cmdBuffer = cmdBuffers[imageIndex];

    VkCommandBufferBeginInfo cmdBufferBeginInfo;
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferBeginInfo.pNext = nullptr;
    cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    cmdBufferBeginInfo.pInheritanceInfo = nullptr;

#ifdef WAIT_PRESENT_FENCE
    vkResetFences(device, 1, &cmdSubmitFences[imageIndex]);
#endif

    vkResetCommandBuffer(cmdBuffer, 0);
    result = vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo);
    assert(VK_SUCCESS == result);
    if (VK_SUCCESS == result)
    {
        std::array<VkClearValue, 2> clearValues;
        clearValues[0].color = {0.35f, 0.53f, 0.7f, 1.f};
        clearValues[1].depthStencil = {1.f, 0};

        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = framebuffer;
        renderPassBeginInfo.renderArea.offset = VkOffset2D{0, 0};
        renderPassBeginInfo.renderArea.extent = VkExtent2D{width, height};
        renderPassBeginInfo.clearValueCount = (uint32_t)clearValues.size();
        renderPassBeginInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        {
            // Empty
        }
        vkCmdEndRenderPass(cmdBuffer);
    }
    vkEndCommandBuffer(cmdBuffer);

    submit(imageIndex);
    present(imageIndex);
    waitForPresentComplete(imageIndex);

    ++frameCount;
    float dt = timer.millisecondsElapsed();
    time += dt;
    if (time > 1000.f)
    {
        fps = (uint32_t)(std::roundf(frameCount * (time / 1000.f)));
        time = 0.f;
        frameCount = 0;

        const std::string caption = "FPS: " + std::to_string(fps);
        SetWindowText(hWnd, caption.c_str());
    }
}

static VkBool32 VKAPI_PTR debugCallback(
    VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
    uint64_t object, size_t location, int32_t messageCode,
    const char *pLayerPrefix, const char *pMessage, void *pUserData)
{
    if (!strstr(pMessage, "Extension"))
    {
        OutputDebugStringA(pMessage);
        OutputDebugStringA("\n");
    }
    return VK_FALSE;
}

void VkApp::createInstance()
{
    std::array<const char *, 4> enabledExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };
    std::array<const char *, 1> enabledLayerNames = {
        "VK_LAYER_KHRONOS_validation"
    };

    VkApplicationInfo appInfo;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "Vulkan";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "VulkanApp";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instanceInfo;
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pNext = nullptr;
    instanceInfo.flags = 0;
    instanceInfo.pApplicationInfo = &appInfo;
#ifdef _DEBUG
    instanceInfo.enabledLayerCount = (uint32_t)enabledLayerNames.size();
    instanceInfo.ppEnabledLayerNames = enabledLayerNames.data();
#else
    instanceInfo.enabledLayerCount = 0;
    instanceInfo.ppEnabledLayerNames = nullptr;
#endif
    instanceInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
    instanceInfo.ppEnabledExtensionNames = enabledExtensions.data();

    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance);
    CHECK_SUCCEEDED(result, "failed to create Vulkan instance");
#ifdef _DEBUG
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    if (vkCreateDebugReportCallbackEXT)
    {
        VkDebugReportCallbackCreateInfoEXT debugReportCallbackInfo;
        debugReportCallbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debugReportCallbackInfo.pNext = nullptr;
        debugReportCallbackInfo.flags =
            VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
            VK_DEBUG_REPORT_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_ERROR_BIT_EXT |
            VK_DEBUG_REPORT_DEBUG_BIT_EXT;
        debugReportCallbackInfo.pfnCallback = debugCallback;
        debugReportCallbackInfo.pUserData = nullptr;
        VkResult result = vkCreateDebugReportCallbackEXT(instance, &debugReportCallbackInfo, nullptr, &debugReportCallback);
        CHECK_SUCCEEDED(result, "failed to create debug report callback");
    }
#endif // _DEBUG
}

void VkApp::createPhysicalDevice()
{
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices;
    physicalDevices.resize(physicalDeviceCount);
    VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
    CHECK_SUCCEEDED(result, "failed to enumerate physical devices");

    physicalDevice = physicalDevices[0];
    uint32_t propertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &propertyCount, nullptr);
    if (propertyCount)
    {
        queueFamilyProperties.resize(propertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &propertyCount, queueFamilyProperties.data());
    }
}

void VkApp::createLogicalDevice()
{
    std::vector<const char *> enabledExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
    };

#ifdef _DEBUG
    uint32_t propertyCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &propertyCount, nullptr);
    if (propertyCount)
    {
        extensionProperties.resize(propertyCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &propertyCount, extensionProperties.data());
    }
    for (const char *extensionName: enabledExtensions)
    {
        if (!findExtension(extensionName))
        {
            OutputDebugString(extensionName);
            OutputDebugString("!\n");
        }
    }
#endif // _DEBUG

    const float defaultQueuePriorities[1] = {1.f};

    VkDeviceQueueCreateInfo graphicsQueueInfo, computeQueueInfo, transferQueueInfo;
    graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphicsQueueInfo.pNext = nullptr;
    graphicsQueueInfo.flags = 0;
    graphicsQueueInfo.queueFamilyIndex = chooseFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    graphicsQueueInfo.queueCount = 1;
    graphicsQueueInfo.pQueuePriorities = defaultQueuePriorities;
    computeQueueInfo = graphicsQueueInfo;
    computeQueueInfo.queueFamilyIndex = chooseFamilyIndex(VK_QUEUE_COMPUTE_BIT);
    transferQueueInfo = graphicsQueueInfo;
    transferQueueInfo.queueFamilyIndex = chooseFamilyIndex(VK_QUEUE_TRANSFER_BIT);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.push_back(graphicsQueueInfo);
    if (computeQueueInfo.queueFamilyIndex != graphicsQueueInfo.queueFamilyIndex)
        queueCreateInfos.push_back(computeQueueInfo);
    if ((transferQueueInfo.queueFamilyIndex != graphicsQueueInfo.queueFamilyIndex) &&
        (transferQueueInfo.queueFamilyIndex != computeQueueInfo.queueFamilyIndex))
        queueCreateInfos.push_back(transferQueueInfo);

    VkDeviceCreateInfo deviceInfo;
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = nullptr;
    deviceInfo.flags = 0;
    deviceInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
    deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceInfo.enabledLayerCount = 0;
    deviceInfo.ppEnabledLayerNames = nullptr;
    deviceInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
    deviceInfo.ppEnabledExtensionNames = enabledExtensions.data();
    deviceInfo.pEnabledFeatures = nullptr;
    VkResult result = vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);
    if (VK_ERROR_EXTENSION_NOT_PRESENT == result)
        throw std::runtime_error("required extension not present");
    CHECK_SUCCEEDED(result, "failed to create device");

    vkGetDeviceQueue(device, graphicsQueueInfo.queueFamilyIndex, 0, &graphicsQueue);
    vkGetDeviceQueue(device, computeQueueInfo.queueFamilyIndex, 0, &computeQueue);
    vkGetDeviceQueue(device, transferQueueInfo.queueFamilyIndex, 0, &transferQueue);
}

void VkApp::createWin32Surface()
{
    VkWin32SurfaceCreateInfoKHR surfaceInfo;
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.pNext = nullptr;
    surfaceInfo.flags = 0;
    surfaceInfo.hinstance = hInstance;
    surfaceInfo.hwnd = hWnd;
    VkResult result = vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface);
    CHECK_SUCCEEDED(result, "failed to create Win32 surface");
}

void VkApp::createSwapchain()
{
    VkSwapchainCreateInfoKHR swapchainInfo;
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.pNext = nullptr;
    swapchainInfo.flags = 0;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = 2;
    swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainInfo.imageExtent = VkExtent2D{width, height};
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;
    swapchainInfo.pQueueFamilyIndices = nullptr;
    swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain);
    CHECK_SUCCEEDED(result, "failed to create swapchain");

    VkImageViewCreateInfo imageViewInfo;
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.pNext = nullptr;
    imageViewInfo.flags = 0;
    //imageViewInfo.image = ;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format = swapchainInfo.imageFormat;
    imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.levelCount = 1;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount = 1;

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
    swapchainImages.resize(swapchainImageCount);
    result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());
    for (auto& image: swapchainImages)
    {
        imageViewInfo.image = image;
        VkImageView imageView = VK_NULL_HANDLE;
        result = vkCreateImageView(device, &imageViewInfo, nullptr, &imageView);
        CHECK_SUCCEEDED(result, "failed to create image view");
        swapchainImageViews.push_back(imageView);
    }
}

void VkApp::createRenderPass()
{
    VkAttachmentReference colorAttachment{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentDescription colorAttachmentDescription;
    colorAttachmentDescription.flags = 0;
    colorAttachmentDescription.format = VK_FORMAT_B8G8R8A8_UNORM;
    colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkSubpassDescription subpassDescription;
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachment;
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;

    VkSubpassDependency subpassBeginDependency;
    subpassBeginDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassBeginDependency.dstSubpass = 0;
    subpassBeginDependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subpassBeginDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassBeginDependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    subpassBeginDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassBeginDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    VkSubpassDependency subpassEndDependency;
    subpassEndDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassEndDependency.dstSubpass = 0;
    subpassEndDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassEndDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subpassEndDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassEndDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    subpassEndDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    VkSubpassDependency dependencies[] = {subpassBeginDependency, subpassEndDependency};

    VkRenderPassCreateInfo renderPassInfo;
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pNext = nullptr;
    renderPassInfo.flags = 0;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachmentDescription;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies;
    VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    CHECK_SUCCEEDED(result, "failed to create render pass");
}

void VkApp::createFramebuffer()
{
    VkFramebufferCreateInfo framebufferInfo;
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext = nullptr;
    framebufferInfo.flags = 0;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = 1;
    //framebufferInfo.pAttachments =
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    framebuffers.resize(swapchainImageViews.size());
    uint32_t i = 0;
    for (VkFramebuffer& framebuffer: framebuffers)
    {
        framebufferInfo.pAttachments = &swapchainImageViews[i++];
        VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer);
        CHECK_SUCCEEDED(result, "failed to create framebuffer");
    }
}

void VkApp::createCommandPools()
{
    VkCommandPoolCreateInfo cmdPoolInfo;
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.pNext = nullptr;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolInfo.queueFamilyIndex = chooseFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    VkResult result = vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &graphicsCmdPool);
    cmdPoolInfo.queueFamilyIndex = chooseFamilyIndex(VK_QUEUE_COMPUTE_BIT);
    result = vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &computeCmdPool);
    CHECK_SUCCEEDED(result, "failed to create graphics command pool");
    cmdPoolInfo.queueFamilyIndex = chooseFamilyIndex(VK_QUEUE_TRANSFER_BIT);
    CHECK_SUCCEEDED(result, "failed to create compute command pool");
    result = vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &transferCmdPool);
    CHECK_SUCCEEDED(result, "failed to create transfer command pool");
}

void VkApp::createCommandBuffers()
{
    VkCommandBufferAllocateInfo cmdBufferAllocateInfo;
    cmdBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufferAllocateInfo.pNext = nullptr;
    cmdBufferAllocateInfo.commandPool = graphicsCmdPool;
    cmdBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufferAllocateInfo.commandBufferCount = (uint32_t)swapchainImages.size();
    cmdBuffers.resize(cmdBufferAllocateInfo.commandBufferCount);
    VkResult result = vkAllocateCommandBuffers(device, &cmdBufferAllocateInfo, cmdBuffers.data());
    CHECK_SUCCEEDED(result, "failed to create graphics command buffers");
    cmdBufferAllocateInfo.commandPool = computeCmdPool;
    cmdBufferAllocateInfo.commandBufferCount = 1;
    result = vkAllocateCommandBuffers(device, &cmdBufferAllocateInfo, &computeCmdBuffer);
    CHECK_SUCCEEDED(result, "failed to create compute command buffer");
    cmdBufferAllocateInfo.commandPool = transferCmdPool;
    cmdBufferAllocateInfo.commandBufferCount = 1;
    result = vkAllocateCommandBuffers(device, &cmdBufferAllocateInfo, &transferCmdBuffer);
    CHECK_SUCCEEDED(result, "failed to create transfer command buffer");
}

void VkApp::createSyncPrimitices()
{
    VkSemaphoreCreateInfo semaphoreInfo;
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;
    VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &presentSemaphore);
    result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
    CHECK_SUCCEEDED(result, "failed to create semephore");

    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

#ifdef WAIT_PRESENT_FENCE
    cmdSubmitFences.resize(cmdBuffers.size());
    for (VkFence& fence: cmdSubmitFences)
        result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
#endif // WAIT_PRESENT_FENCE

    result = vkCreateFence(device, &fenceInfo, nullptr, &transferFence);
    CHECK_SUCCEEDED(result, "failed to create fence");
}

void VkApp::submit(uint32_t imageIndex)
{
    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &presentSemaphore;
    submitInfo.pWaitDstStageMask = &waitDstStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphore; // Will be signaled when the command buffers for this batch have completed execution

#ifdef WAIT_PRESENT_FENCE
    VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, cmdSubmitFences[imageIndex]);
#else
    VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
#endif
    CHECK_SUCCEEDED(result, "queue submission failed");
}

void VkApp::present(uint32_t imageIndex)
{
    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore; // Wait for command buffers have completed execution before issuing the present request
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    VkResult result = vkQueuePresentKHR(graphicsQueue, &presentInfo);
    CHECK_SUCCEEDED(result, "present failed");
}

void VkApp::waitForPresentComplete(uint32_t imageIndex)
{
#ifdef WAIT_PRESENT_FENCE
    constexpr uint64_t timeout = 10 * 1000000; // 10 ms
    VkResult result = vkWaitForFences(device, 1, &cmdSubmitFences[imageIndex], VK_FALSE, timeout);
    if (VK_TIMEOUT == result)
        OutputDebugStringA("timeout has expired\n");
    else
        CHECK_SUCCEEDED(result, "wait for fence failed");
#else
    VkResult result = vkDeviceWaitIdle(device);
    assert(VK_SUCCESS == result);
    CHECK_SUCCEEDED(result, "wait for device to become idle failed");
#endif // !WAIT_PRESENT_FENCE
}

bool VkApp::findExtension(const char *extensionName) const
{
    auto it = std::find_if(extensionProperties.begin(), extensionProperties.end(),
        [extensionName](auto const& property)
        {
            return 0 == strcmp(property.extensionName, extensionName);
        });
    return it != extensionProperties.end();
}

uint32_t VkApp::chooseFamilyIndex(VkQueueFlagBits queueType) const
{
    uint32_t queueFamilyIndex = 0;
    if (VK_QUEUE_COMPUTE_BIT == queueType)
    {
        for (auto const& property: queueFamilyProperties)
        {
            if (property.queueFlags & queueType)
            {
                const VkFlags hasGraphics = property.queueFlags & VK_QUEUE_GRAPHICS_BIT;
                if (!hasGraphics)
                    return queueFamilyIndex;
            }
            ++queueFamilyIndex;
        }
    }
    else if ((VK_QUEUE_TRANSFER_BIT == queueType) || (VK_QUEUE_SPARSE_BINDING_BIT == queueType))
    {
        for (auto const& property: queueFamilyProperties)
        {
            if (property.queueFlags & queueType)
            {
                const VkFlags hasGraphicsOrCompute = property.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
                if (!hasGraphicsOrCompute)
                    return queueFamilyIndex;
            }
            ++queueFamilyIndex;
        }
    }
    queueFamilyIndex = 0;
    for (auto const& property: queueFamilyProperties)
    {   // Try to find any suitable family
        if (property.queueFlags & queueType)
            return queueFamilyIndex;
        ++queueFamilyIndex;
    }
    return 0;
}

std::unique_ptr<Win32App> appFactory(const Win32App::Entry& entry)
{
    return std::make_unique<VkApp>(entry, "Vulkan", SCREEN_WIDTH, SCREEN_HEIGHT);
}
