#pragma once

#include "DepthTexture.h"
#include "SubRenderer.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

const uint32_t maxFramesInFlight = 2;
const int START_RENDERING_PRIORITY = 2000;

namespace vecs {

	// Forward Declarations
	class Engine;
	class Device;
	struct RefreshWindowEvent;

	class Renderer {
	friend class SubRenderer;
	public:
		GLFWwindow* window;

		VkQueue graphicsQueue;
		VkQueue presentQueue;

		uint32_t imageCount = 0;
		VkExtent2D swapChainExtent;

		Renderer(Engine* engine) {
			this->engine = engine;
		}

		void init(Device* device, VkSurfaceKHR surface, GLFWwindow* window);
		void acquireImage();
		void presentImage(std::vector<SubRenderer*>* subrenderers);

		void cleanup();

	private:
		Engine* engine;
		Device* device;
		VkSurfaceKHR surface;

		VkSurfaceFormatKHR surfaceFormat;
		VkFormat swapChainImageFormat;
		VkPresentModeKHR presentMode;

		DepthTexture depthTexture;
		VkRenderPass renderPass;

		VkSwapchainKHR swapChain;
		std::vector<VkImage> swapChainImages;
		std::vector<VkImageView> swapChainImageViews;
		std::vector<VkFramebuffer> swapChainFramebuffers;
		std::vector<VkCommandBuffer> commandBuffers;

		size_t currentFrame = 0;
		uint32_t imageIndex;
		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;
		std::vector<VkFence> imagesInFlight;

		void createRenderPass();

		void refreshWindow(RefreshWindowEvent* ignored);
		bool createSwapChain(VkSwapchainKHR* oldSwapChain = nullptr);
		void createFramebuffers();
		void createImageViews();

		void buildCommandBuffer(std::vector<SubRenderer*>* subrenderers);

		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	};
}