#include "application.h"
#include "darray.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define WIDTH 1280
#define HEIGHT 720

const Vertex vertices[] = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
};

const uint16_t indices[] = {
    0, 1, 2, 2, 3, 0, // plane 1
    4, 5, 6, 6, 7, 4, // plane 2
};

#define VK_CHECK(fun)                                                          \
    if (fun != VK_SUCCESS) {                                                   \
        fprintf(stderr, "%s:%d: \"%s\"\n", __FILE_NAME__, __LINE__, #fun);     \
        exit(EXIT_FAILURE);                                                    \
    }

const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
#define validation_layer_count sizeof(validation_layers) / sizeof(const char *)

const char *device_extenstions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#define device_extenstion_count                                                \
    sizeof(device_extenstions) / sizeof(const char *)

#ifdef NDEBUG
#define enable_validation_layers false
#else
#define enable_validation_layers true
#endif

#define check_validation()                                                     \
    if (enable_validation_layers && !check_validation_layer_support()) {       \
        fprintf(stderr,                                                        \
                "%s:%d: validation layers requested, but not available\n",     \
                __FILE_NAME__, __LINE__);                                      \
        exit(EXIT_FAILURE);                                                    \
    }

typedef struct {
    mat4s model;
    mat4s view;
    mat4s projection;
} UniformBufferObject;

typedef struct {
    uint32_t graphics_family;
    bool has_graphics_family;
    uint32_t present_family;
    bool has_present_family;
} QueueFamilyIndices;

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    uint32_t format_count;
    VkPresentModeKHR *present_modes;
    uint32_t present_mode_count;
} SwapchainSupportDetails;

static void recreate_swapchain(Application *app);
static void create_instance(Application *app);
static void setup_debug_messenger(Application *app);
static void create_surface(Application *app);
static void pick_physical_device(Application *app);
static void create_logical_device(Application *app);
static void cleanup_swapchain(Application *app);
static void create_swapchain(Application *app);
static void create_image_views(Application *app);
static void create_render_pass(Application *app);
static void create_descriptor_set_layout(Application *app);
static void create_graphics_pipeline(Application *app);
static void create_command_pool(Application *app);
static void create_depth_resources(Application *app);
static void create_framebuffers(Application *app);
static void create_texture_image(Application *app);
static void create_texture_image_view(Application *app);
static void create_texture_sampler(Application *app);
static void create_vertex_buffer(Application *app);
static void create_index_buffer(Application *app);
static void create_uniform_buffer(Application *app);
static void create_descriptor_pool(Application *app);
static void create_descriptor_sets(Application *app);
static void create_command_buffers(Application *app);
static void create_sync_objects(Application *app);
static void record_command_buffer(const Application *app,
                                  VkCommandBuffer command_buffer,
                                  uint32_t image_index);
static void update_uniform_buffer(const Application *app,
                                  uint32_t current_image);
static void draw_frame(Application *app);
static int rate_device_suitability(Application *app, VkPhysicalDevice device);
static QueueFamilyIndices findQueueFamilies(Application *app,
                                            VkPhysicalDevice device);
static bool queue_family_indices_is_complete(QueueFamilyIndices indices);
static uint32_t *queue_family_indices_get_unique(QueueFamilyIndices indices);
static SwapchainSupportDetails query_swapchain_support(Application *app,
                                                       VkPhysicalDevice device);
static void
swapchain_support_details_destroy(SwapchainSupportDetails swapchain_support);
static VkSurfaceFormatKHR
choose_swap_surface_format(const VkSurfaceFormatKHR *available_formats,
                           uint32_t available_format_count);
static VkPresentModeKHR
choose_swap_present_mode(const VkPresentModeKHR *available_present_modes,
                         uint32_t available_present_mode_count);
static VkExtent2D choose_swap_extent(Application *app,
                                     VkSurfaceCapabilitiesKHR capabilities);
static VkVertexInputBindingDescription vertex_get_binding_description();
static VkVertexInputAttributeDescription *vertex_get_attribute_descriptions();
static bool check_validation_layer_support();
static bool check_device_extension_support(VkPhysicalDevice device);
static uint32_t *read_file(const char *file_name, size_t *out_size);
static VkShaderModule create_shader_module(const Application *app,
                                           const uint32_t *code,
                                           size_t code_size);
static void create_buffer(const Application *app, VkDeviceSize size,
                          VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties, VkBuffer *buffer,
                          VkDeviceMemory *buffer_memory);
static void copy_buffer(const Application *app, VkBuffer src_buffer,
                        VkBuffer dst_buffer, VkDeviceSize size);
static void create_image(const Application *app, uint32_t width,
                         uint32_t height, VkFormat format, VkImageTiling tiling,
                         VkImageUsageFlags usage,
                         VkMemoryPropertyFlags properties, VkImage *image,
                         VkDeviceMemory *image_memory);
static void transition_image_layout(const Application *app, VkImage image,
                                    VkFormat format, VkImageLayout old_layout,
                                    VkImageLayout new_layout);
static void copy_buffer_to_image(const Application *app, VkBuffer buffer,
                                 VkImage image, uint32_t width,
                                 uint32_t height);
static VkCommandBuffer begin_single_time_commands(const Application *app);
static void end_single_time_commands(const Application *app,
                                     VkCommandBuffer command_buffer);
static VkImageView create_image_view(const Application *app, VkImage image,
                                     VkFormat format,
                                     VkImageAspectFlags aspect_flags);
static void populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT *create_info);
static const char **get_required_extensions();
static uint32_t find_memory_type(VkPhysicalDevice physical_device,
                                 uint32_t type_filter,
                                 VkMemoryPropertyFlags properties);
static VkFormat find_supported_format(VkPhysicalDevice physical_device,
                                      VkFormat *candidates,
                                      uint32_t candidate_count,
                                      VkImageTiling tiling,
                                      VkFormatFeatureFlags features);
static bool has_stencil_component(VkFormat format);
static VkFormat find_depth_format(VkPhysicalDevice physical_device);
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);
static void framebuffer_resize_callback(GLFWwindow *window, int width,
                                        int height);

VkResult create_debug_utils_messenger_ext(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void destroy_debug_utils_messenger_ext(
    VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *pAllocator) {
    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL) {
        func(instance, debugMessenger, pAllocator);
    }
}

void init_window(Application *app) {
    if (glfwInit() != GLFW_TRUE) {
        fprintf(stderr, "Failed to initialize GLFW!\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    app->window = glfwCreateWindow(WIDTH, HEIGHT, "game", NULL, NULL);
    glfwSetWindowUserPointer(app->window, app);
    glfwSetFramebufferSizeCallback(app->window, framebuffer_resize_callback);
}

void init_vulkan(Application *app) {
    create_instance(app);
    setup_debug_messenger(app);
    create_surface(app);
    pick_physical_device(app);
    create_logical_device(app);
    create_swapchain(app);
    create_image_views(app);
    create_render_pass(app);
    create_descriptor_set_layout(app);
    create_graphics_pipeline(app);
    create_command_pool(app);
    create_depth_resources(app);
    create_framebuffers(app);
    create_texture_image(app);
    create_texture_image_view(app);
    create_texture_sampler(app);
    create_vertex_buffer(app);
    create_index_buffer(app);
    create_uniform_buffer(app);
    create_descriptor_pool(app);
    create_descriptor_sets(app);
    create_command_buffers(app);
    create_sync_objects(app);
}

void main_loop(Application *app) {
    double start_time = glfwGetTime();
    double prev_frame_time = start_time;
    while (!glfwWindowShouldClose(app->window)) {
        double current_time = glfwGetTime();
        app->time = current_time - start_time;
        app->delta_time = current_time - prev_frame_time;

        glfwPollEvents();
        draw_frame(app);

        prev_frame_time = app->current_frame;
    }

    vkDeviceWaitIdle(app->device);
}

void cleanup(Application *app) {
    cleanup_swapchain(app);

    vkDestroySampler(app->device, app->texture_sampler, NULL);
    vkDestroyImageView(app->device, app->texture_image_view, NULL);
    vkDestroyImage(app->device, app->texture_image, NULL);
    vkFreeMemory(app->device, app->texture_image_memory, NULL);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(app->device, app->uniform_buffers[i], NULL);
        vkFreeMemory(app->device, app->uniform_buffers_memory[i], NULL);
    }

    vkDestroyDescriptorPool(app->device, app->descriptor_pool, NULL);

    vkDestroyDescriptorSetLayout(app->device, app->descriptor_set_layout, NULL);

    vkDestroyBuffer(app->device, app->index_buffer, NULL);
    vkFreeMemory(app->device, app->index_buffer_memory, NULL);

    vkDestroyBuffer(app->device, app->vertex_buffer, NULL);
    vkFreeMemory(app->device, app->vertex_buffer_memory, NULL);

    vkDestroyPipeline(app->device, app->graphics_pipeline, NULL);
    vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);

    vkDestroyRenderPass(app->device, app->render_pass, NULL);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(app->device, app->image_available_semaphores[i],
                           NULL);
        vkDestroySemaphore(app->device, app->render_finished_semaphores[i],
                           NULL);
        vkDestroyFence(app->device, app->in_flight_fences[i], NULL);
    }

    vkDestroyCommandPool(app->device, app->command_pool, NULL);

    vkDestroyDevice(app->device, NULL);

    vkDestroySurfaceKHR(app->instance, app->surface, NULL);

    if (enable_validation_layers) {
        destroy_debug_utils_messenger_ext(app->instance, app->debug_messenger,
                                          NULL);
    }

    vkDestroyInstance(app->instance, NULL);

    glfwDestroyWindow(app->window);

    glfwTerminate();
}

static void cleanup_swapchain(Application *app) {
    vkDestroyImageView(app->device, app->depth_image_view, NULL);
    vkDestroyImage(app->device, app->depth_image, NULL);
    vkFreeMemory(app->device, app->depth_image_memory, NULL);

    for (int i = 0; i < app->swapchain_image_count; i++) {
        vkDestroyFramebuffer(app->device, app->swapchain_framebuffers[i], NULL);
    }

    for (int i = 0; i < app->swapchain_image_count; i++) {
        vkDestroyImageView(app->device, app->swapchain_image_views[i], NULL);
    }

    free(app->swapchain_images);
    free(app->swapchain_image_views);
    free(app->swapchain_framebuffers);

    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
}

static void recreate_swapchain(Application *app) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(app->window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(app->window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(app->device);

    cleanup_swapchain(app);

    create_swapchain(app);
    create_image_views(app);
    create_depth_resources(app);
    create_framebuffers(app);
}

static void create_instance(Application *app) {
    check_validation();

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello, Triangle",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    const char **required_extensions = get_required_extensions();

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = darray_length(required_extensions),
        .ppEnabledExtensionNames = required_extensions,
    };

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
    if (enable_validation_layers) {
        create_info.enabledLayerCount = validation_layer_count;
        create_info.ppEnabledLayerNames = validation_layers;

        populate_debug_messenger_create_info(&debug_create_info);
        create_info.pNext = &debug_create_info;
    } else {
        create_info.enabledLayerCount = 0;

        create_info.pNext = NULL;
    }

    VK_CHECK(vkCreateInstance(&create_info, NULL, &app->instance));
    darray_destroy(required_extensions);
}

static void setup_debug_messenger(Application *app) {
    if (!enable_validation_layers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT create_info;
    populate_debug_messenger_create_info(&create_info);

    VK_CHECK(create_debug_utils_messenger_ext(app->instance, &create_info, NULL,
                                              &app->debug_messenger));
}

static void create_surface(Application *app) {
    VK_CHECK(glfwCreateWindowSurface(app->instance, app->window, NULL,
                                     &app->surface));
}

static void pick_physical_device(Application *app) {
    app->physical_device = VK_NULL_HANDLE;

    uint32_t device_count;
    vkEnumeratePhysicalDevices(app->instance, &device_count, NULL);

    if (device_count == 0) {
        fprintf(stderr, "failed to find GPU with vulkan support\n");
        exit(EXIT_FAILURE);
    }

    VkPhysicalDevice devices[device_count];
    vkEnumeratePhysicalDevices(app->instance, &device_count, devices);

    int max_rating = 0;
    for (uint32_t i = 0; i < device_count; ++i) {
        int rating;
        if ((rating = rate_device_suitability(app, devices[i])) > max_rating) {
            app->physical_device = devices[i];
            max_rating = rating;
            break;
        }
    }

    if (app->physical_device == VK_NULL_HANDLE) {
        fprintf(stderr, "failed to find suitable GPU\n");
        exit(EXIT_FAILURE);
    }
}

static void create_logical_device(Application *app) {
    QueueFamilyIndices indices = findQueueFamilies(app, app->physical_device);

    uint32_t *unique_indices = queue_family_indices_get_unique(indices);

    VkDeviceQueueCreateInfo queue_create_infos[darray_length(unique_indices)];

    float queue_priority = 1.0f;
    for (int i = 0; i < darray_length(unique_indices); i++) {
        queue_create_infos[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique_indices[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
    }

    VkPhysicalDeviceFeatures device_features = {
        .samplerAnisotropy = VK_TRUE,
    };

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_create_infos,
        .queueCreateInfoCount = darray_length(unique_indices),
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = device_extenstion_count,
        .ppEnabledExtensionNames = device_extenstions,
    };

    if (enable_validation_layers) {
        create_info.enabledLayerCount = validation_layer_count;
        create_info.ppEnabledLayerNames = validation_layers;
    } else {
        create_info.enabledLayerCount = 0;
    }

    VK_CHECK(
        vkCreateDevice(app->physical_device, &create_info, NULL, &app->device));

    vkGetDeviceQueue(app->device, indices.graphics_family, 0,
                     &app->graphics_queue);
    vkGetDeviceQueue(app->device, indices.present_family, 0,
                     &app->present_queue);

    darray_destroy(unique_indices);
}

static void create_swapchain(Application *app) {
    SwapchainSupportDetails swapchain_support =
        query_swapchain_support(app, app->physical_device);

    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(
        swapchain_support.formats, swapchain_support.format_count);
    VkPresentModeKHR present_mode = choose_swap_present_mode(
        swapchain_support.present_modes, swapchain_support.present_mode_count);
    VkExtent2D extent = choose_swap_extent(app, swapchain_support.capabilities);
    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
    if (swapchain_support.capabilities.maxImageCount > 0 &&
        image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = app->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = swapchain_support.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    QueueFamilyIndices indices = findQueueFamilies(app, app->physical_device);
    uint32_t queue_family_indices[] = {indices.graphics_family,
                                       indices.present_family};

    if (indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }

    VK_CHECK(
        vkCreateSwapchainKHR(app->device, &create_info, NULL, &app->swapchain));

    vkGetSwapchainImagesKHR(app->device, app->swapchain,
                            &app->swapchain_image_count, NULL);
    app->swapchain_images = calloc(app->swapchain_image_count, sizeof(VkImage));
    vkGetSwapchainImagesKHR(app->device, app->swapchain,
                            &app->swapchain_image_count, app->swapchain_images);

    app->swapchain_image_format = surface_format.format;
    app->swapchain_extent = extent;

    swapchain_support_details_destroy(swapchain_support);
}

static void create_image_views(Application *app) {
    app->swapchain_image_views =
        calloc(app->swapchain_image_count, sizeof(VkImageView));

    for (size_t i = 0; i < app->swapchain_image_count; i++) {
        app->swapchain_image_views[i] = create_image_view(
            app, app->swapchain_images[i], app->swapchain_image_format,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

static void create_render_pass(Application *app) {
    VkAttachmentDescription color_attachment = {
        .format = app->swapchain_image_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_attachment_reference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentDescription depth_attachment = {
        .format = find_depth_format(app->physical_device),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depth_attachment_reference = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_reference,
        .pDepthStencilAttachment = &depth_attachment_reference,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkAttachmentDescription attachments[] = {color_attachment,
                                             depth_attachment};

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount =
            sizeof(attachments) / sizeof(VkAttachmentDescription),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    VK_CHECK(vkCreateRenderPass(app->device, &render_pass_info, NULL,
                                &app->render_pass));
}

static void create_descriptor_set_layout(Application *app) {
    VkDescriptorSetLayoutBinding ubo_layout_binding = {
        .binding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImmutableSamplers = NULL,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };

    VkDescriptorSetLayoutBinding sampler_layout_binding = {
        .binding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImmutableSamplers = NULL,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutBinding bindings[] = {ubo_layout_binding,
                                               sampler_layout_binding};

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = sizeof(bindings) / sizeof(VkDescriptorSetLayoutBinding),
        .pBindings = bindings,
    };

    VK_CHECK(vkCreateDescriptorSetLayout(app->device, &layout_info, NULL,
                                         &app->descriptor_set_layout));
}

static void create_graphics_pipeline(Application *app) {
    size_t vert_shader_size;
    uint32_t *vert_shader_code =
        read_file("shaders/simple_shader.vert.spv", &vert_shader_size);
    size_t frag_shader_size;
    uint32_t *frag_shader_code =
        read_file("shaders/simple_shader.frag.spv", &frag_shader_size);

    VkShaderModule vert_shader_module =
        create_shader_module(app, vert_shader_code, vert_shader_size);
    VkShaderModule frag_shader_module =
        create_shader_module(app, frag_shader_code, frag_shader_size);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert_shader_module,
        .pName = "main",
    };

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag_shader_module,
        .pName = "main",
    };

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info,
                                                       frag_shader_stage_info};

    VkVertexInputBindingDescription binding_description =
        vertex_get_binding_description();
    VkVertexInputAttributeDescription *attribute_descriptions =
        vertex_get_attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount =
            darray_length(attribute_descriptions),
        .pVertexAttributeDescriptions = attribute_descriptions,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    uint32_t dynamic_state_count =
        sizeof(dynamic_states) / sizeof(VkDynamicState);

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamic_state_count,
        .pDynamicStates = dynamic_states,
    };

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)app->swapchain_extent.width,
        .height = (float)app->swapchain_extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = app->swapchain_extent,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0f,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &app->descriptor_set_layout,
    };

    VK_CHECK(vkCreatePipelineLayout(app->device, &pipeline_layout_info, NULL,
                                    &app->pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = app->pipeline_layout,
        .renderPass = app->render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    VK_CHECK(vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1,
                                       &pipeline_info, NULL,
                                       &app->graphics_pipeline));

    vkDestroyShaderModule(app->device, vert_shader_module, NULL);
    vkDestroyShaderModule(app->device, frag_shader_module, NULL);

    free(vert_shader_code);
    free(frag_shader_code);
}

static void create_command_pool(Application *app) {
    QueueFamilyIndices queue_family_indices =
        findQueueFamilies(app, app->physical_device);

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_family_indices.graphics_family,
    };

    VK_CHECK(
        vkCreateCommandPool(app->device, &pool_info, NULL, &app->command_pool));
}

static void create_depth_resources(Application *app) {
    VkFormat depth_format = find_depth_format(app->physical_device);

    create_image(app, app->swapchain_extent.width, app->swapchain_extent.height,
                 depth_format, VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->depth_image,
                 &app->depth_image_memory);
    app->depth_image_view = create_image_view(
        app, app->depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

    transition_image_layout(app, app->depth_image, depth_format,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

static void create_framebuffers(Application *app) {
    app->swapchain_framebuffers =
        calloc(app->swapchain_image_count, sizeof(VkFramebuffer));

    for (int i = 0; i < app->swapchain_image_count; i++) {
        VkImageView attachments[] = {
            app->swapchain_image_views[i],
            app->depth_image_view,
        };

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = app->render_pass,
            .attachmentCount = sizeof(attachments) / sizeof(VkImageView),
            .pAttachments = attachments,
            .width = app->swapchain_extent.width,
            .height = app->swapchain_extent.height,
            .layers = 1,
        };

        VK_CHECK(vkCreateFramebuffer(app->device, &framebuffer_info, NULL,
                                     &app->swapchain_framebuffers[i]));
    }
}

static void create_texture_image(Application *app) {
    int texture_width, texture_height, texture_channels;
    stbi_uc *pixels =
        stbi_load("textures/texture.jpg", &texture_width, &texture_height,
                  &texture_channels, STBI_rgb_alpha);
    VkDeviceSize image_size = texture_width * texture_height * 4;

    if (!pixels) {
        fprintf(stderr, "Failed to load texture image!\n");
        exit(EXIT_FAILURE);
    }

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    create_buffer(app, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  &staging_buffer, &staging_buffer_memory);

    void *data;
    vkMapMemory(app->device, staging_buffer_memory, 0, image_size, 0, &data);
    memcpy(data, pixels, image_size);
    vkUnmapMemory(app->device, staging_buffer_memory);

    stbi_image_free(pixels);

    create_image(app, texture_width, texture_height, VK_FORMAT_R8G8B8A8_SRGB,
                 VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->texture_image,
                 &app->texture_image_memory);

    transition_image_layout(app, app->texture_image, VK_FORMAT_R8G8B8A8_SRGB,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(app, staging_buffer, app->texture_image, texture_width,
                         texture_height);
    transition_image_layout(app, app->texture_image, VK_FORMAT_R8G8B8A8_SRGB,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(app->device, staging_buffer, NULL);
    vkFreeMemory(app->device, staging_buffer_memory, NULL);
}

static void create_texture_image_view(Application *app) {
    app->texture_image_view =
        create_image_view(app, app->texture_image, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_ASPECT_COLOR_BIT);
}

static void create_texture_sampler(Application *app) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(app->physical_device, &properties);

    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };

    VK_CHECK(vkCreateSampler(app->device, &sampler_info, NULL,
                             &app->texture_sampler));
}

static void create_vertex_buffer(Application *app) {
    VkDeviceSize buffer_size = sizeof(vertices);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    create_buffer(app, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  &staging_buffer, &staging_buffer_memory);

    void *data;
    vkMapMemory(app->device, staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, vertices, buffer_size);
    vkUnmapMemory(app->device, staging_buffer_memory);

    create_buffer(app, buffer_size,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->vertex_buffer,
                  &app->vertex_buffer_memory);

    copy_buffer(app, staging_buffer, app->vertex_buffer, buffer_size);

    vkDestroyBuffer(app->device, staging_buffer, NULL);
    vkFreeMemory(app->device, staging_buffer_memory, NULL);
}

static void create_index_buffer(Application *app) {
    VkDeviceSize buffer_size = sizeof(indices);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    create_buffer(app, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  &staging_buffer, &staging_buffer_memory);

    void *data;
    vkMapMemory(app->device, staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, indices, buffer_size);
    vkUnmapMemory(app->device, staging_buffer_memory);

    create_buffer(app, buffer_size,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->index_buffer,
                  &app->index_buffer_memory);

    copy_buffer(app, staging_buffer, app->index_buffer, buffer_size);

    vkDestroyBuffer(app->device, staging_buffer, NULL);
    vkFreeMemory(app->device, staging_buffer_memory, NULL);
}

static void create_uniform_buffer(Application *app) {
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        create_buffer(app, buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &app->uniform_buffers[i],
                      &app->uniform_buffers_memory[i]);
        vkMapMemory(app->device, app->uniform_buffers_memory[i], 0, buffer_size,
                    0, &app->uniform_buffers_mapped[i]);
    }
}

static void create_descriptor_pool(Application *app) {
    VkDescriptorPoolSize pool_sizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT,
        },
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize),
        .pPoolSizes = pool_sizes,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
    };

    VK_CHECK(vkCreateDescriptorPool(app->device, &pool_info, NULL,
                                    &app->descriptor_pool));
}

static void create_descriptor_sets(Application *app) {
    VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        layouts[i] = app->descriptor_set_layout;
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = app->descriptor_pool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts,
    };

    VK_CHECK(vkAllocateDescriptorSets(app->device, &alloc_info,
                                      app->descriptor_sets));

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = app->uniform_buffers[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject),
        };

        VkDescriptorImageInfo image_info = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = app->texture_image_view,
            .sampler = app->texture_sampler,
        };

        VkWriteDescriptorSet descriptor_writes[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = app->descriptor_sets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &buffer_info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = app->descriptor_sets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = &image_info,
            },
        };

        vkUpdateDescriptorSets(app->device,
                               sizeof(descriptor_writes) /
                                   sizeof(VkWriteDescriptorSet),
                               descriptor_writes, 0, NULL);
    }
}

static void create_command_buffers(Application *app) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    };

    VK_CHECK(vkAllocateCommandBuffers(app->device, &alloc_info,
                                      app->command_buffers));
}

static void create_sync_objects(Application *app) {
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateSemaphore(app->device, &semaphore_info, NULL,
                                   &app->image_available_semaphores[i]));
        VK_CHECK(vkCreateSemaphore(app->device, &semaphore_info, NULL,
                                   &app->render_finished_semaphores[i]));
        VK_CHECK(vkCreateFence(app->device, &fence_info, NULL,
                               &app->in_flight_fences[i]));
    }
}

static void record_command_buffer(const Application *app,
                                  VkCommandBuffer command_buffer,
                                  uint32_t image_index) {
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));

    VkClearValue clear_values[] = {
        (VkClearValue){.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
        (VkClearValue){.depthStencil = {1.0f, 0}},
    };

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = app->render_pass,
        .framebuffer = app->swapchain_framebuffers[image_index],
        .renderArea = {.offset = {0, 0}, .extent = app->swapchain_extent},
        .clearValueCount = sizeof(clear_values) / sizeof(VkClearValue),
        .pClearValues = clear_values,
    };

    vkCmdBeginRenderPass(command_buffer, &render_pass_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      app->graphics_pipeline);

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)app->swapchain_extent.width,
        .height = (float)app->swapchain_extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = app->swapchain_extent,
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    VkBuffer vertex_buffers[] = {app->vertex_buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

    vkCmdBindIndexBuffer(command_buffer, app->index_buffer, 0,
                         VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            app->pipeline_layout, 0, 1,
                            &app->descriptor_sets[app->current_frame], 0, NULL);

    vkCmdDrawIndexed(command_buffer, sizeof(indices) / sizeof(uint16_t), 1, 0,
                     0, 0);

    vkCmdEndRenderPass(command_buffer);

    VK_CHECK(vkEndCommandBuffer(command_buffer));
}

static void update_uniform_buffer(const Application *app,
                                  uint32_t current_image) {
    UniformBufferObject ubo = {
        .model = glms_rotate(glms_mat4_identity(), app->time * glm_rad(90.0f),
                             (vec3s){0.0f, 0.0f, 1.0f}),
        .view =
            glms_lookat((vec3s){2.0f, 2.0f, 2.0f}, (vec3s){0.0f, 0.0f, 0.0f},
                        (vec3s){0.0f, 0.0f, 1.0f}),
        .projection = glms_perspective(glm_rad(45.0f),
                                       (float)app->swapchain_extent.width /
                                           (float)app->swapchain_extent.height,
                                       0.1f, 10.0f),
    };

    ubo.projection.m11 *= -1;

    memcpy(app->uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
}

static void draw_frame(Application *app) {
    vkWaitForFences(app->device, 1, &app->in_flight_fences[app->current_frame],
                    VK_TRUE, UINT64_MAX);

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(
        app->device, app->swapchain, UINT64_MAX,
        app->image_available_semaphores[app->current_frame], VK_NULL_HANDLE,
        &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain(app);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "Failed to acquire swapchain image!\n");
        exit(EXIT_FAILURE);
    }

    vkResetFences(app->device, 1, &app->in_flight_fences[app->current_frame]);

    vkResetCommandBuffer(app->command_buffers[app->current_frame], 0);
    record_command_buffer(app, app->command_buffers[app->current_frame],
                          image_index);

    update_uniform_buffer(app, app->current_frame);

    VkSemaphore wait_semaphores[] = {
        app->image_available_semaphores[app->current_frame]};
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSemaphore signal_semaphores[] = {
        app->render_finished_semaphores[app->current_frame]};

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &app->command_buffers[app->current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores,
    };

    VK_CHECK(vkQueueSubmit(app->graphics_queue, 1, &submit_info,
                           app->in_flight_fences[app->current_frame]));

    VkSwapchainKHR swapchains[] = {app->swapchain};

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &image_index,
        .pResults = NULL,
    };

    result = vkQueuePresentKHR(app->present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        app->framebuffer_resized) {
        app->framebuffer_resized = false;
        recreate_swapchain(app);
    } else
        VK_CHECK(result);

    app->current_frame = (app->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

static int rate_device_suitability(Application *app, VkPhysicalDevice device) {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);

    QueueFamilyIndices indices = findQueueFamilies(app, device);

    if (!queue_family_indices_is_complete(indices)) {
        return -1;
    }

    if (!check_device_extension_support(device)) {
        return -1;
    }

    SwapchainSupportDetails swapchain_support =
        query_swapchain_support(app, device);
    if (!swapchain_support.format_count ||
        !swapchain_support.present_mode_count) {
        swapchain_support_details_destroy(swapchain_support);
        return -1;
    }
    swapchain_support_details_destroy(swapchain_support);

    if (!device_features.geometryShader || !device_features.samplerAnisotropy) {
        return -1;
    }

    int score = 0;

    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 10000;
    }

    // Maximum possible size of textures.
    score += device_properties.limits.maxImageDimension2D;

    printf("%-40s | %d\n", device_properties.deviceName, score);
    return score;
}

static QueueFamilyIndices findQueueFamilies(Application *app,
                                            VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

    VkQueueFamilyProperties queue_families[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             queue_families);

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.has_graphics_family = true;
        }

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, app->surface,
                                             &present_support);
        if (present_support) {
            indices.present_family = i;
            indices.has_present_family = true;
        }

        if (queue_family_indices_is_complete(indices)) {
            break;
        }
    }

    return indices;
}

static bool queue_family_indices_is_complete(QueueFamilyIndices indices) {
    return indices.has_graphics_family && indices.has_present_family;
}

static uint32_t *queue_family_indices_get_unique(QueueFamilyIndices indices) {
    uint32_t index_numbers[] = {indices.graphics_family,
                                indices.present_family};
    size_t index_number_count = sizeof(index_numbers) / sizeof(uint32_t);
    uint32_t *unique_indices = darray_create(uint32_t);

    for (int i = 0; i < index_number_count; i++) {
        bool is_unique = true;
        for (int j = 0; j < darray_length(unique_indices); j++) {
            if (index_numbers[i] == unique_indices[j]) {
                is_unique = false;
                break;
            }
        }
        if (is_unique) {
            darray_push(unique_indices, index_numbers[i]);
        }
    }

    return unique_indices;
}

static SwapchainSupportDetails
query_swapchain_support(Application *app, VkPhysicalDevice device) {
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, app->surface,
                                              &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, app->surface,
                                         &details.format_count, NULL);

    if (details.format_count != 0) {
        details.formats =
            calloc(details.format_count, sizeof(VkSurfaceFormatKHR));
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, app->surface, &details.format_count, details.formats);
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, app->surface, &details.present_mode_count, NULL);

    if (details.present_mode_count != 0) {
        details.present_modes =
            calloc(details.present_mode_count, sizeof(VkPresentModeKHR));
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->surface,
                                                  &details.present_mode_count,
                                                  details.present_modes);
    }

    return details;
}

static void
swapchain_support_details_destroy(SwapchainSupportDetails swapchain_support) {
    free(swapchain_support.present_modes);
    free(swapchain_support.formats);
}

static VkSurfaceFormatKHR
choose_swap_surface_format(const VkSurfaceFormatKHR *available_formats,
                           uint32_t available_format_count) {
    for (int i = 0; i < available_format_count; i++) {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            available_formats[i].colorSpace ==
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_formats[i];
        }
    }

    return available_formats[0];
}

static VkPresentModeKHR
choose_swap_present_mode(const VkPresentModeKHR *available_present_modes,
                         uint32_t available_present_mode_count) {
    for (int i = 0; i < available_present_mode_count; i++) {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_modes[i];
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_swap_extent(Application *app,
                                     VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(app->window, &width, &height);
    VkExtent2D actual_extent = {
        .width = CLAMP(width, capabilities.minImageExtent.width,
                       capabilities.maxImageExtent.width),
        .height = CLAMP(height, capabilities.minImageExtent.height,
                        capabilities.maxImageExtent.height),
    };

    return actual_extent;
}

static VkVertexInputBindingDescription vertex_get_binding_description() {
    return (VkVertexInputBindingDescription){
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
}

static VkVertexInputAttributeDescription *vertex_get_attribute_descriptions() {
    VkVertexInputAttributeDescription *attribute_descriptions =
        darray_create(VkVertexInputAttributeDescription);

    VkVertexInputAttributeDescription attribute_description = {
        .binding = 0,
        .location = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, position),
    };

    darray_push(attribute_descriptions, attribute_description);

    attribute_description = (VkVertexInputAttributeDescription){
        .binding = 0,
        .location = 1,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, color),
    };

    darray_push(attribute_descriptions, attribute_description);

    attribute_description = (VkVertexInputAttributeDescription){
        .binding = 0,
        .location = 2,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, texture_coordinate),
    };

    darray_push(attribute_descriptions, attribute_description);

    return attribute_descriptions;
}

static bool check_validation_layer_support() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);

    VkLayerProperties available_layers[layer_count];
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

    for (int i = 0; i < validation_layer_count; i++) {
        bool layer_found = false;

        for (int j = 0; j < layer_count; j++) {
            if (strcmp(validation_layers[i], available_layers[j].layerName) ==
                0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            return false;
        }
    }

    return true;
}

static bool check_device_extension_support(VkPhysicalDevice device) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

    VkExtensionProperties available_extensions[extension_count];
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count,
                                         available_extensions);

    for (int i = 0; i < device_extenstion_count; i++) {
        bool extension_found = false;

        for (int j = 0; j < extension_count; j++) {
            if (strcmp(device_extenstions[i],
                       available_extensions[j].extensionName) == 0) {
                extension_found = true;
                break;
            }
        }

        if (!extension_found) {
            return false;
        }
    }

    return true;
}

static uint32_t *read_file(const char *file_name, size_t *out_size) {
    FILE *fp = fopen(file_name, "rb");

    if (fp == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    fseek(fp, 0, SEEK_END);
    *out_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint32_t *buffer = malloc(*out_size);
    fread(buffer, sizeof(char), *out_size, fp);

    fclose(fp);
    return buffer;
}

static VkShaderModule create_shader_module(const Application *app,
                                           const uint32_t *code,
                                           size_t code_size) {
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code_size,
        .pCode = code,
    };

    VkShaderModule shader_module;

    VK_CHECK(
        vkCreateShaderModule(app->device, &create_info, NULL, &shader_module));

    return shader_module;
}

static void create_buffer(const Application *app, VkDeviceSize size,
                          VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties, VkBuffer *buffer,
                          VkDeviceMemory *buffer_memory) {
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VK_CHECK(vkCreateBuffer(app->device, &buffer_info, NULL, buffer));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(app->device, *buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex =
            find_memory_type(app->physical_device,
                             memory_requirements.memoryTypeBits, properties),
    };

    VK_CHECK(vkAllocateMemory(app->device, &alloc_info, NULL, buffer_memory));

    vkBindBufferMemory(app->device, *buffer, *buffer_memory, 0);
}

static void copy_buffer(const Application *app, VkBuffer src_buffer,
                        VkBuffer dst_buffer, VkDeviceSize size) {
    VkCommandBuffer command_buffer = begin_single_time_commands(app);

    VkBufferCopy copy_region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size,
    };

    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

    end_single_time_commands(app, command_buffer);
}

static void create_image(const Application *app, uint32_t width,
                         uint32_t height, VkFormat format, VkImageTiling tiling,
                         VkImageUsageFlags usage,
                         VkMemoryPropertyFlags properties, VkImage *image,
                         VkDeviceMemory *image_memory) {
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = {.width = width, .height = height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = format,
        .tiling = tiling,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
    };

    VK_CHECK(vkCreateImage(app->device, &image_info, NULL, image));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(app->device, *image, &memory_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex =
            find_memory_type(app->physical_device,
                             memory_requirements.memoryTypeBits, properties),
    };

    VK_CHECK(vkAllocateMemory(app->device, &alloc_info, NULL, image_memory));

    vkBindImageMemory(app->device, *image, *image_memory, 0);
}

static void transition_image_layout(const Application *app, VkImage image,
                                    VkFormat format, VkImageLayout old_layout,
                                    VkImageLayout new_layout) {
    VkCommandBuffer command_buffer = begin_single_time_commands(app);

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            {
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (has_stencil_component(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
               new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        fprintf(stderr, "Unsupported layout transition: %d -> %d", old_layout,
                new_layout);
        exit(EXIT_FAILURE);
    }

    vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage,
                         0,          // dependencies
                         0, NULL,    // memory barriers
                         0, NULL,    // buffer memory barriers
                         1, &barrier // image memory barriers
    );

    end_single_time_commands(app, command_buffer);
}

static void copy_buffer_to_image(const Application *app, VkBuffer buffer,
                                 VkImage image, uint32_t width,
                                 uint32_t height) {
    VkCommandBuffer command_buffer = begin_single_time_commands(app);

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .mipLevel = 0,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1},
    };

    vkCmdCopyBufferToImage(command_buffer, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    end_single_time_commands(app, command_buffer);
}

static VkCommandBuffer begin_single_time_commands(const Application *app) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = app->command_pool,
        .commandBufferCount = 1,
    };

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(app->device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

static void end_single_time_commands(const Application *app,
                                     VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };

    vkQueueSubmit(app->graphics_queue, 1, &submit_info, NULL);
    vkQueueWaitIdle(app->graphics_queue);

    vkFreeCommandBuffers(app->device, app->command_pool, 1, &command_buffer);
}

static VkImageView create_image_view(const Application *app, VkImage image,
                                     VkFormat format,
                                     VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange =
            {
                .aspectMask = aspect_flags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkImageView image_view;
    VK_CHECK(vkCreateImageView(app->device, &view_info, NULL, &image_view));

    return image_view;
}

static void populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT *create_info) {
    *create_info = (VkDebugUtilsMessengerCreateInfoEXT){
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
    };
}

/**
 * @return darray
 */
static const char **get_required_extensions() {
    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions;

    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    const char **required_extensions = darray_create(const char *);
    for (int i = 0; i < glfw_extension_count; ++i) {
        darray_push(required_extensions, glfw_extensions[i]);
    }

    if (enable_validation_layers) {
        const char *debug_extension_name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        darray_push(required_extensions, debug_extension_name);
    }

    return required_extensions;
}

static uint32_t find_memory_type(VkPhysicalDevice physical_device,
                                 uint32_t type_filter,
                                 VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            ((memory_properties.memoryTypes[i].propertyFlags & properties) ==
             properties)) {
            return i;
        }
    }

    fprintf(stderr, "Failed to find suitable memory type\n");
    exit(EXIT_FAILURE);
}

static VkFormat find_supported_format(VkPhysicalDevice physical_device,
                                      VkFormat *candidates,
                                      uint32_t candidate_count,
                                      VkImageTiling tiling,
                                      VkFormatFeatureFlags features) {
    for (int i = 0; i < candidate_count; i++) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(physical_device, candidates[i],
                                            &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (properties.linearTilingFeatures & features) == features) {
            return candidates[i];
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                   (properties.optimalTilingFeatures & features) == features) {
            return candidates[i];
        }
    }

    fprintf(stderr, "Failed to find supported format!\n");
    exit(EXIT_FAILURE);
}

static VkFormat find_depth_format(VkPhysicalDevice physical_device) {

    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };

    return find_supported_format(
        physical_device, candidates, sizeof(candidates) / sizeof(VkFormat),
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static bool has_stencil_component(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           format == VK_FORMAT_D24_UNORM_S8_UINT;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_type,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data) {
    fprintf(stderr, "validation layer: %s\n", callback_data->pMessage);

    return VK_FALSE;
}

static void framebuffer_resize_callback(GLFWwindow *window, int width,
                                        int height) {
    Application *app = (Application *)glfwGetWindowUserPointer(window);
    app->framebuffer_resized = true;
}
