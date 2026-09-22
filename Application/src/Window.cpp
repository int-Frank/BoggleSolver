// SDL3 + Vulkan + Dear ImGui framework code: window/device setup, per-frame bookkeeping, teardown.
// Vulkan setup is intentionally minimal: single graphics queue, no MSAA, no depth buffer.
// Structure follows Dear ImGui's examples/example_sdl3_vulkan/main.cpp.

#include "ApplicationAPI.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstring>

namespace
{
  VkAllocationCallbacks*   g_Allocator = nullptr;
  VkInstance               g_Instance = VK_NULL_HANDLE;
  VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
  VkDevice                 g_Device = VK_NULL_HANDLE;
  uint32_t                 g_QueueFamily = static_cast<uint32_t>(-1);
  VkQueue                  g_Queue = VK_NULL_HANDLE;
  VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

  ImGui_ImplVulkanH_Window g_MainWindowData;
  uint32_t                 g_MinImageCount = 2;
  bool                     g_SwapChainRebuild = false;

  SDL_Window* g_Window = nullptr;
  bool        g_Done = false;

  void CheckVkResult(VkResult err)
  {
    if (err == VK_SUCCESS)
      return;
    std::fprintf(stderr, "[vulkan] Error: VkResult = %d\n", static_cast<int>(err));
    if (err < 0)
      std::abort();
  }

  bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension)
  {
    for (const VkExtensionProperties& p : properties)
      if (std::strcmp(p.extensionName, extension) == 0)
        return true;
    return false;
  }

  void SetupVulkan(ImVector<const char*> instanceExtensions)
  {
    VkResult err;

    // Create Vulkan instance.
    {
      uint32_t propertiesCount = 0;
      ImVector<VkExtensionProperties> properties;
      vkEnumerateInstanceExtensionProperties(nullptr, &propertiesCount, nullptr);
      properties.resize(static_cast<int>(propertiesCount));
      err = vkEnumerateInstanceExtensionProperties(nullptr, &propertiesCount, properties.Data);
      CheckVkResult(err);

      if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

      VkInstanceCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.Size);
      createInfo.ppEnabledExtensionNames = instanceExtensions.Data;
      err = vkCreateInstance(&createInfo, g_Allocator, &g_Instance);
      CheckVkResult(err);
    }

    g_PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g_Instance);
    IM_ASSERT(g_PhysicalDevice != VK_NULL_HANDLE);

    g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_PhysicalDevice);
    IM_ASSERT(g_QueueFamily != static_cast<uint32_t>(-1));

    // Create logical device with a single graphics queue.
    {
      ImVector<const char*> deviceExtensions;
      deviceExtensions.push_back("VK_KHR_swapchain");

      const float queuePriority[] = { 1.0f };
      VkDeviceQueueCreateInfo queueInfo[1]{};
      queueInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueInfo[0].queueFamilyIndex = g_QueueFamily;
      queueInfo[0].queueCount = 1;
      queueInfo[0].pQueuePriorities = queuePriority;

      VkDeviceCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      createInfo.queueCreateInfoCount = 1;
      createInfo.pQueueCreateInfos = queueInfo;
      createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.Size);
      createInfo.ppEnabledExtensionNames = deviceExtensions.Data;
      err = vkCreateDevice(g_PhysicalDevice, &createInfo, g_Allocator, &g_Device);
      CheckVkResult(err);
      vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
    }

    // Descriptor pool sized for ImGui (fonts + a handful of user textures).
    {
      VkDescriptorPoolSize poolSizes[] =
      {
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE },
        { VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
      };
      VkDescriptorPoolCreateInfo poolInfo{};
      poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
      poolInfo.maxSets = 0;
      for (const VkDescriptorPoolSize& poolSize : poolSizes)
        poolInfo.maxSets += poolSize.descriptorCount;
      poolInfo.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes));
      poolInfo.pPoolSizes = poolSizes;
      err = vkCreateDescriptorPool(g_Device, &poolInfo, g_Allocator, &g_DescriptorPool);
      CheckVkResult(err);
    }
  }

  void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
  {
    VkBool32 supportsPresent = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, surface, &supportsPresent);
    if (supportsPresent != VK_TRUE)
    {
      std::fprintf(stderr, "Error: no WSI support on the selected physical device.\n");
      std::exit(EXIT_FAILURE);
    }

    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->Surface = surface;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat, static_cast<int>(IM_ARRAYSIZE(requestSurfaceImageFormat)), requestSurfaceColorSpace);

    VkPresentModeKHR presentModes[] = { VK_PRESENT_MODE_FIFO_KHR };
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, wd->Surface, presentModes, IM_ARRAYSIZE(presentModes));

    IM_ASSERT(g_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount, 0);
  }

  void CleanupVulkan()
  {
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);
  }

  void CleanupVulkanWindow(ImGui_ImplVulkanH_Window* wd)
  {
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, wd, g_Allocator);
    vkDestroySurfaceKHR(g_Instance, wd->Surface, g_Allocator);
  }

  void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* drawData)
  {
    VkSemaphore imageAcquiredSemaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore renderCompleteSemaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

    VkResult err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
      g_SwapChainRebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
      return;
    if (err != VK_SUBOPTIMAL_KHR)
      CheckVkResult(err);

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
      err = vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
      CheckVkResult(err);
      err = vkResetFences(g_Device, 1, &fd->Fence);
      CheckVkResult(err);
    }
    {
      err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
      CheckVkResult(err);
      VkCommandBufferBeginInfo beginInfo{};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      err = vkBeginCommandBuffer(fd->CommandBuffer, &beginInfo);
      CheckVkResult(err);
    }
    {
      VkRenderPassBeginInfo renderPassInfo{};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderPassInfo.renderPass = wd->RenderPass;
      renderPassInfo.framebuffer = fd->Framebuffer;
      renderPassInfo.renderArea.extent.width = static_cast<uint32_t>(wd->Width);
      renderPassInfo.renderArea.extent.height = static_cast<uint32_t>(wd->Height);
      renderPassInfo.clearValueCount = 1;
      renderPassInfo.pClearValues = &wd->ClearValue;
      vkCmdBeginRenderPass(fd->CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    ImGui_ImplVulkan_RenderDrawData(drawData, fd->CommandBuffer);

    vkCmdEndRenderPass(fd->CommandBuffer);
    {
      VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      VkSubmitInfo submitInfo{};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = &imageAcquiredSemaphore;
      submitInfo.pWaitDstStageMask = &waitStage;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &fd->CommandBuffer;
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = &renderCompleteSemaphore;

      err = vkEndCommandBuffer(fd->CommandBuffer);
      CheckVkResult(err);
      err = vkQueueSubmit(g_Queue, 1, &submitInfo, fd->Fence);
      CheckVkResult(err);
    }
  }

  void FramePresent(ImGui_ImplVulkanH_Window* wd)
  {
    if (g_SwapChainRebuild)
      return;

    VkSemaphore renderCompleteSemaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderCompleteSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &wd->Swapchain;
    presentInfo.pImageIndices = &wd->FrameIndex;

    VkResult err = vkQueuePresentKHR(g_Queue, &presentInfo);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
      g_SwapChainRebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
      return;
    if (err != VK_SUBOPTIMAL_KHR)
      CheckVkResult(err);

    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
  }
}

namespace App
{
  bool InitApplication()
  {
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
      std::fprintf(stderr, "Error: SDL_Init(): %s\n", SDL_GetError());
      return false;
    }

    SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    g_Window = SDL_CreateWindow("Boggle Solver", 1280, 800, windowFlags);
    if (g_Window == nullptr)
    {
      std::fprintf(stderr, "Error: SDL_CreateWindow(): %s\n", SDL_GetError());
      return false;
    }

    ImVector<const char*> instanceExtensions;
    {
      uint32_t sdlExtensionsCount = 0;
      const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionsCount);
      for (uint32_t n = 0; n < sdlExtensionsCount; n++)
        instanceExtensions.push_back(sdlExtensions[n]);
    }
    SetupVulkan(instanceExtensions);

    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(g_Window, g_Instance, g_Allocator, &surface))
    {
      std::fprintf(stderr, "Error: SDL_Vulkan_CreateSurface(): %s\n", SDL_GetError());
      return false;
    }

    int fbWidth, fbHeight;
    SDL_GetWindowSizeInPixels(g_Window, &fbWidth, &fbHeight);
    SetupVulkanWindow(&g_MainWindowData, surface, fbWidth, fbHeight);

    SDL_SetWindowPosition(g_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(g_Window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForVulkan(g_Window);
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = g_Instance;
    initInfo.PhysicalDevice = g_PhysicalDevice;
    initInfo.Device = g_Device;
    initInfo.QueueFamily = g_QueueFamily;
    initInfo.Queue = g_Queue;
    initInfo.DescriptorPool = g_DescriptorPool;
    initInfo.MinImageCount = g_MinImageCount;
    initInfo.ImageCount = g_MainWindowData.ImageCount;
    initInfo.Allocator = g_Allocator;
    initInfo.PipelineInfoMain.RenderPass = g_MainWindowData.RenderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.CheckVkResultFn = CheckVkResult;
    ImGui_ImplVulkan_Init(&initInfo);

    return true;
  }

  bool IsMinimised()
  {
    return (SDL_GetWindowFlags(g_Window) & SDL_WINDOW_MINIMIZED) != 0;
  }

  bool IsDone()
  {
    return g_Done;
  }

  void BeginFrame()
  {
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT)
        g_Done = true;
      if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(g_Window))
        g_Done = true;
    }

    if (IsMinimised())
    {
      SDL_Delay(10);
      return;
    }

    int fbWidth, fbHeight;
    SDL_GetWindowSizeInPixels(g_Window, &fbWidth, &fbHeight);
    if (fbWidth > 0 && fbHeight > 0 && (g_SwapChainRebuild || g_MainWindowData.Width != fbWidth || g_MainWindowData.Height != fbHeight))
    {
      ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
      ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, fbWidth, fbHeight, g_MinImageCount, 0);
      g_MainWindowData.FrameIndex = 0;
      g_SwapChainRebuild = false;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
  }

  void EndFrame()
  {
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    const bool isMinimized = (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f);
    if (!isMinimized)
    {
      g_MainWindowData.ClearValue.color.float32[0] = 0.05f;
      g_MainWindowData.ClearValue.color.float32[1] = 0.05f;
      g_MainWindowData.ClearValue.color.float32[2] = 0.08f;
      g_MainWindowData.ClearValue.color.float32[3] = 1.0f;
      FrameRender(&g_MainWindowData, drawData);
      FramePresent(&g_MainWindowData);
    }
  }

  bool Shutdown()
  {
    VkResult err = vkDeviceWaitIdle(g_Device);
    CheckVkResult(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow(&g_MainWindowData);
    CleanupVulkan();

    SDL_DestroyWindow(g_Window);
    SDL_Quit();

    return true;
  }
}
