#include "vkCmdCenter.h"
#include "vkCommon.h"
#include "vkDeviceContext.h"
#include "vkFrameContext.h"
#include "vkSurface.h"
#include "vkQueue.h"
#include "vkSwapchain.h"

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback
(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* userData)
{
    fprintf(stderr, "[Vulkan] %s\n", data->pMessage);
    return VK_FALSE;
}

vkCmdCenter::vkCmdCenter()
    : instance(VK_NULL_HANDLE)
{
    device_context = std::make_shared<vkDeviceContext>();
    frame_context = std::make_shared<vkFrameContext>();
    surface = std::make_unique<vkSurface>();
    std::ranges::for_each(queue, [](auto& q)
        {
            q = std::make_unique<vkQueue>();
        });
}

vkCmdCenter::~vkCmdCenter()
{
    clear();
	if (instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(instance, nullptr);
	}
    surface.release();
}

bool vkCmdCenter::initialize(std::string_view application_name, GLFWwindow* window, const u32 width, const u32 height)
{
    // init volk
    volk_init();
    // create instance
    create_instance(application_name);
    // create surface
    create_surface(window);
	// physical device
    if (!create_physical_device())
        return false;
    // logical device
    if (!create_logical_device())
        return false;
    // create swapchain
    if (!create_swapchain(width, height))
        return false;

    return rhiCmdCenter::initialize(application_name, window, width, height);
}

void vkCmdCenter::volk_init()
{
    VK_CHECK_ERROR(volkInitialize());
}

void vkCmdCenter::create_instance(std::string_view application_name)
{
    VkApplicationInfo app_info
    {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = application_name.data(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "vkEngine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    const auto extensions = get_required_extensions();

    VkInstanceCreateInfo create_info
    {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = static_cast<u32>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    std::array<const char*, 1> layer_names = { "VK_LAYER_KHRONOS_validation" };
    if (has_layer(layer_names.data(), static_cast<u32>(layer_names.size())))
    {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = layer_names.data();
    }
    else 
    {
        create_info.enabledLayerCount = 0;
    }

    VkValidationFeaturesEXT validataion_features{};
    std::vector<VkValidationFeatureEnableEXT> enables = {
        VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
        // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        // VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
    };
    validataion_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validataion_features.enabledValidationFeatureCount = static_cast<u32>(enables.size());
    validataion_features.pEnabledValidationFeatures = enables.data();

    VkDebugUtilsMessengerCreateInfoEXT dbg_createinfo{};
    dbg_createinfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dbg_createinfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;   // 필요 시 VERBOSE도 추가
    dbg_createinfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbg_createinfo.pfnUserCallback = debug_callback;
    dbg_createinfo.pUserData = nullptr;
    validataion_features.pNext = &dbg_createinfo;
    create_info.pNext = &validataion_features;

    VK_CHECK_ERROR(vkCreateInstance(&create_info, nullptr, &instance));
    volkLoadInstance(instance);

    VkDebugUtilsMessengerEXT dbg{};
    auto vkCreateDebugUtilsMessengerEXT_fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessengerEXT_fn) 
    {
        vkCreateDebugUtilsMessengerEXT_fn(instance, &dbg_createinfo, nullptr, &dbg);
    }
}

std::vector<const char*> vkCmdCenter::get_required_extensions()
{
    u32 glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

bool vkCmdCenter::has_layer(const char** layers, const u32 size)
{
    u32 layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const auto& layer_prop : available_layers)
    {
        for (u32 index = 0; index < size; ++index)
        {
            const char* layer_name = layers[index];
            if (strcmp(layer_prop.layerName, layer_name) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

void vkCmdCenter::create_surface(GLFWwindow* window)
{
    VK_HANDLE_CHECK(instance);
    surface->create_surface(instance, window);
}

bool vkCmdCenter::create_physical_device()
{
    VK_HANDLE_CHECK(instance);
    auto vk_context = std::static_pointer_cast<vkDeviceContext>(device_context);
    
    u32 device_count = 0;
    VK_CHECK_ERROR(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
    if (device_count == 0)
    {
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    VK_CHECK_ERROR(vkEnumeratePhysicalDevices(instance, &device_count, devices.data()));
    
    for (const auto& device : devices)
    {
        if (is_device_suitable(device))
        {
            vk_context->phys_device = device;
            break;
        }
    }
    VK_HANDLE_CHECK(vk_context->phys_device);
    queue_family_indices = find_queue_families(vk_context->phys_device);

    return true;
}

bool vkCmdCenter::is_device_suitable(VkPhysicalDevice device)
{
    queueFamilyIndices indices = find_queue_families(device);
    return indices.is_complete();
}

vkCmdCenter::queueFamilyIndices vkCmdCenter::find_queue_families(VkPhysicalDevice device)
{
    queueFamilyIndices indices;

    u32 queue_family_property_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_property_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_property_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_property_count, queue_families.data());

    for (u32 i = 0; i < queue_family_property_count; ++i) 
    {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
            indices.families[static_cast<u32>(queueFamilyIndices::queue_family_type::graphics)] = i;

        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            indices.families[static_cast<u32>(queueFamilyIndices::queue_family_type::compute)] = i;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface->get_surface(), &presentSupport);
        if (presentSupport) 
            indices.families[static_cast<u32>(queueFamilyIndices::queue_family_type::present)] = i;

        if (indices.is_complete()) 
            break;
    }

    return indices;
}

bool vkCmdCenter::create_logical_device()
{
    auto vk_context = std::static_pointer_cast<vkDeviceContext>(device_context);
    VK_HANDLE_CHECK(vk_context->phys_device);

    auto rng_uniq_families = queue_family_indices.families
        | std::views::filter([](auto const& f) { return f.has_value(); })
        | std::views::transform([](auto const& f) { return f.value(); });
    auto uniq_families = std::vector<u32>(std::ranges::begin(rng_uniq_families), std::ranges::end(rng_uniq_families));
    const bool all_same = std::ranges::all_of(uniq_families, [first = uniq_families.front()](u32 v) 
        { 
            return v == first; 
        });

    const f32 queue_priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_create_info;
    queue_create_info.reserve(uniq_families.size());
    if (all_same)
    {
        queue_family_indices.priorities[0] = std::vector<float>(uniq_families.size(), 1.f);
        VkDeviceQueueCreateInfo create_info
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = uniq_families.front(),
            .queueCount = static_cast<u32>(uniq_families.size()),
            .pQueuePriorities = queue_family_indices.priorities[0].data()
        };
        queue_create_info.push_back(create_info);
    }
    else
    {
        std::ranges::sort(uniq_families);
        uniq_families.erase(std::ranges::unique(uniq_families).begin(), uniq_families.end());
        for (const u32 index : uniq_families)
        {
            queue_family_indices.priorities[index] = std::vector<float>(1, 1.f);
            VkDeviceQueueCreateInfo create_info
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = index,
                .queueCount = 1,
                .pQueuePriorities = queue_family_indices.priorities[index].data()
            };
            queue_create_info.push_back(create_info);
        }
    }

    std::vector<const char*> device_extension_names = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkPhysicalDeviceFeatures device_features{};
    VkDeviceCreateInfo device_create_info
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<u32>(queue_create_info.size()),
        .pQueueCreateInfos = queue_create_info.data(),
        .enabledExtensionCount = static_cast<u32>(device_extension_names.size()),
        .ppEnabledExtensionNames = device_extension_names.data(),
        .pEnabledFeatures = &device_features,
    };
    
    VkPhysicalDeviceVulkan11Features v11{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
    };
    VkPhysicalDeviceVulkan12Features v12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
    };
    VkPhysicalDeviceVulkan13Features v13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    };
    v11.pNext = &v12;
    v12.pNext = &v13;
    VkPhysicalDeviceFeatures2 device_features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &v11
    };
    vkGetPhysicalDeviceFeatures2(vk_context->phys_device, &device_features2);
    v13.dynamicRendering = VK_TRUE;
    v13.synchronization2 = VK_TRUE;
    v12.descriptorIndexing = VK_TRUE;
    v12.runtimeDescriptorArray = VK_TRUE;
    v12.descriptorBindingPartiallyBound = VK_TRUE;
    v12.descriptorBindingVariableDescriptorCount = VK_TRUE;
    v12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    v12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    v12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    v12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    v12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    v12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    v12.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE;
    v12.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE;
    v11.shaderDrawParameters = VK_TRUE;

    device_create_info.pEnabledFeatures = nullptr;
    device_create_info.pNext = &device_features2;

    VK_CHECK_ERROR(vkCreateDevice(vk_context->phys_device, &device_create_info, nullptr, &vk_context->device));
    for (auto type : enum_range_to_sentinel<queueFamilyIndices::queue_family_type, queueFamilyIndices::queue_family_type::count>())
    {
        if (!queue_family_indices.families[static_cast<i32>(type)].has_value())
            continue;

        queue[static_cast<i32>(type)]->create_queue(vk_context->device, queue_family_indices.families[static_cast<i32>(type)].value());
    }

    volkLoadDevice(vk_context->device);

    vk_context->init_after_createdevice(
        instance, 
        queue[static_cast<i32>(queueFamilyIndices::queue_family_type::graphics)]->handle(), 
        queue_family_indices.families[static_cast<i32>(queueFamilyIndices::queue_family_type::graphics)].value());

    return true;
}

bool vkCmdCenter::create_swapchain(const u32 width, const u32 height)
{
    assert(swapchain == nullptr);

    const u32 gfx_queue_findex = queue_family_indices.families[static_cast<u32>(queueFamilyIndices::queue_family_type::graphics)].value();
    const u32 present_queue_findex =queue_family_indices.families[static_cast<u32>(queueFamilyIndices::queue_family_type::present)].value();
    const auto present_queue = queue[static_cast<i32>(queueFamilyIndices::queue_family_type::present)]->handle();

    vkSwapChainCreateDesc desc{
        .context = std::dynamic_pointer_cast<vkDeviceContext>(device_context).get(),
        .surface = surface->get_surface(),
        .gfx_family = gfx_queue_findex,
        .present_family = present_queue_findex,
        .present_queue = present_queue,
        .width = width,
        .height = height
    };
    swapchain = std::make_unique<vkSwapchain>(std::move(desc));
    init_frame_context();
    return true;
}

void vkCmdCenter::init_frame_context()
{
    const u32 gfx_queue_findex = queue_family_indices.families[static_cast<u32>(queueFamilyIndices::queue_family_type::graphics)].value();
    auto gfx_queue = queue[static_cast<u32>(queueFamilyIndices::queue_family_type::graphics)]->handle();

    auto vk_device_context = std::static_pointer_cast<vkDeviceContext>(device_context);
    auto vk_frame_context = std::static_pointer_cast<vkFrameContext>(frame_context);
    vk_frame_context->swapchain = swapchain.get();
    vk_frame_context->update_inflight(vk_device_context, swapchain->get_swapchain_image_count(), gfx_queue_findex);
    vk_frame_context->create_graphics_queue(vk_device_context->device, gfx_queue, gfx_queue_findex);
}