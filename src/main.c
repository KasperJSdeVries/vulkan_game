#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "darray.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 1280
#define HEIGHT 720

#define VK_CHECK(fun)                                                          \
    if (fun != VK_SUCCESS) {                                                   \
        fprintf(stderr, "%s:%d: \"%s\"\n", __FILE_NAME__, __LINE__, #fun);     \
        exit(EXIT_FAILURE);                                                    \
    }

#define CLAMP(value, min, max) (value < min ? min : (value > max ? max : value))

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

typedef struct {
    GLFWwindow *window;
    VkSurfaceKHR surface;

    VkInstance instance;

    VkDebugUtilsMessengerEXT debug_messenger;

    VkPhysicalDevice physical_device;
    VkDevice device;

    VkQueue graphics_queue;
    VkQueue present_queue;

    VkSwapchainKHR swapchain;
    uint32_t swapchain_image_count;
    VkImage *swapchain_images;
    VkImageView *swapchain_image_views;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
} Application;

static void init_window(Application *app);
static void init_vulkan(Application *app);
static void main_loop(Application *app);
static void cleanup(Application *app);
static void create_instance(Application *app);
static void setup_debug_messenger(Application *app);
static void create_surface(Application *app);
static void pick_physical_device(Application *app);
static void create_logical_device(Application *app);
static void create_swapchain(Application *app);
static void create_image_views(Application *app);
static void create_render_pass(Application *app);
static void create_graphics_pipeline(Application *app);
static int rate_device_suitability(Application *app, VkPhysicalDevice device);
static QueueFamilyIndices findQueueFamilies(Application *app,
                                            VkPhysicalDevice device);
static bool queue_family_indices_is_complete(QueueFamilyIndices indices);
static uint32_t *queue_family_indices_get_unique(QueueFamilyIndices indices);
static SwapchainSupportDetails query_swapchain_support(Application *app,
                                                       VkPhysicalDevice device);
static VkSurfaceFormatKHR
choose_swap_surface_format(const VkSurfaceFormatKHR *available_formats,
                           uint32_t available_format_count);
static VkPresentModeKHR
choose_swap_present_mode(const VkPresentModeKHR *available_present_modes,
                         uint32_t available_present_mode_count);
static VkExtent2D choose_swap_extent(Application *app,
                                     VkSurfaceCapabilitiesKHR capabilities);
static bool check_validation_layer_support();
static bool check_device_extension_support(VkPhysicalDevice device);
static uint32_t *read_file(const char *file_name, size_t *out_size);
static VkShaderModule create_shader_module(const Application *app,
                                           const uint32_t *code,
                                           size_t code_size);
static void populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT *create_info);
static const char **get_required_extensions();
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);

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

int main() {
    Application app = {0};

    init_window(&app);
    init_vulkan(&app);

    main_loop(&app);

    cleanup(&app);
}

static void init_window(Application *app) {
    if (glfwInit() != GLFW_TRUE) {
        fprintf(stderr, "Failed to initialize GLFW!\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    app->window = glfwCreateWindow(WIDTH, HEIGHT, "game", NULL, NULL);
}

static void init_vulkan(Application *app) {
    create_instance(app);
    setup_debug_messenger(app);
    create_surface(app);
    pick_physical_device(app);
    create_logical_device(app);
    create_swapchain(app);
    create_image_views(app);
    create_render_pass(app);
    create_graphics_pipeline(app);
}

static void main_loop(Application *app) {
    while (!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();
    }
}

static void cleanup(Application *app) {
    vkDestroyPipeline(app->device, app->graphics_pipeline, NULL);
    vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);
    vkDestroyRenderPass(app->device, app->render_pass, NULL);

    for (int i = 0; i < app->swapchain_image_count; i++) {
        vkDestroyImageView(app->device, app->swapchain_image_views[i], NULL);
    }

    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);

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

    VkPhysicalDeviceFeatures device_features = {0};

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
}

static void create_image_views(Application *app) {
    app->swapchain_image_views =
        calloc(app->swapchain_image_count, sizeof(VkImageView));

    for (size_t i = 0; i < app->swapchain_image_count; i++) {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = app->swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = app->swapchain_image_format,
            .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1},
        };

        VK_CHECK(vkCreateImageView(app->device, &create_info, NULL,
                                   &app->swapchain_image_views[i]));
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

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_reference,
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    VK_CHECK(vkCreateRenderPass(app->device, &render_pass_info, NULL,
                                &app->render_pass));
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

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
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
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
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

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
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
        .pDepthStencilState = NULL,
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
        return -1;
    }

    if (!device_features.geometryShader) {
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
            calloc(details.format_count, sizeof(VkPresentModeKHR));
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->surface,
                                                  &details.present_mode_count,
                                                  details.present_modes);
    }

    return details;
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

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_type,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data) {
    fprintf(stderr, "validation layer: %s\n", callback_data->pMessage);

    return VK_FALSE;
}
