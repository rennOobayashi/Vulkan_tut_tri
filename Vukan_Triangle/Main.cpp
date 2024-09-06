#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <map>
#include <optional>

#ifdef NDEBUG
	const bool enable_validation_layers = false;
#else 
	const bool enable_validation_layers = true;
#endif

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validation_layers = {
	"VK_LAYER_KHRONOS_validation"
};


VkResult Create_debug_utils_messenger_EXT(VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* p_create_info, 
		const VkAllocationCallbacks* p_allocator, 
		VkDebugUtilsMessengerEXT* p_debug_messenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	//함수가 로드되지 않을 시 함수 반환
	if (func != nullptr)
		return func(instance, p_create_info, p_allocator, p_debug_messenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void destroy_debug_utils_messenger_EXT(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
		const VkAllocationCallbacks* p_allocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		func(instance, debug_messenger, p_allocator);
}

struct queue_family_indices {
	std::optional<uint32_t> graphics_family;

	bool is_complete() {
		return graphics_family.has_value();
	}
};

class Application {
public:
	void run() {
		init_window();
		init_vulkan();
		main_loop();
		cleanup();
	}

private:
	GLFWwindow* window;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;

	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;

	VkQueue graphics_queue;

	void init_window() {
		//glfw 초기화
		glfwInit();

		//OpenGL이 컨택스 되지 않도록 지시
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		//창 초기회
		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void init_vulkan() {
		//vulkan 라이브러리 초기화
		create_instance();

		setup_debug_messenger();
		pick_physical_device();
		create_logical_device();
	}

	void main_loop() {
		//프로그램 종료 확인
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}
	}

	void cleanup() {
		//검색한 장치 정보 파괴
		vkDestroyDevice(device, nullptr);

		if (enable_validation_layers)
			destroy_debug_utils_messenger_EXT(instance, debug_messenger, nullptr);

		//VKInstance를 프로그램이 종료되기 전 파기
		vkDestroyInstance(instance, nullptr);

		//종료시 리소스 정리
		glfwDestroyWindow(window);

		glfwTerminate();
	}

	void create_instance() {
		if (enable_validation_layers && !check_validation_layers_support()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		/*애플리케이션에 대한 일부 정보를 구조체로 채움
		(특정 응용프로그램을 최적화하기 위해 드라이버에 유용한 정보 제공)*/
		VkApplicationInfo app_info{};

		//맴버 유형 명시적 지정
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Hello Triangle";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		//전역확장 및 유효성 검사 레이어
		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;

		auto extensions = get_required_extensions();
		create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		create_info.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
		if (enable_validation_layers) {
			create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
			create_info.ppEnabledLayerNames = validation_layers.data();

			populate_debug_messenger_create_info(debug_create_info);
			create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debug_create_info;
		}
		else {
			create_info.enabledLayerCount = 0;
			create_info.pNext = nullptr;
		}

		if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS)
			throw std::runtime_error("failed to create instance!");
	}

	void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& create_info) {
		create_info = {};

		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		//모든 유형의 심각도를 지정
		create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		//콜백에 어떤 유형의 메세지가 알림으로 전송되는지 필터링
		create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		//콜백 함수에 대한 포인터 지정
		create_info.pfnUserCallback = debugCallback;
	}

	void setup_debug_messenger() {
		if (!enable_validation_layers) return;

		VkDebugUtilsMessengerCreateInfoEXT create_info;
		populate_debug_messenger_create_info(create_info);

		if (Create_debug_utils_messenger_EXT(instance, &create_info, nullptr, &debug_messenger) != VK_SUCCESS)
			throw std::runtime_error("failed to set up debug messenger!");
	}

	std::vector<const char*> get_required_extensions() {
		uint32_t glfw_extension_count = 0;
		const char** glfw_extensions;
		glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

		std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

		if (enable_validation_layers)
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); //오타를 피하게 도와주는 메크로

		return extensions;
	}

	//모든 검증 레이어가 사용 가능한지 체크
	bool check_validation_layers_support() {
		//모든 레이어를 나열
		uint32_t layer_count;
		vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

		//사용 가능한 모든 레이어 나열
		std::vector<VkLayerProperties> available_layers(layer_count);
		vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

		//목록에 있는 모든 레이어가 있는지 확인
		for (const char* layer_name : validation_layers) {
			bool layer_found = false;

			for (const auto& layer_properties : available_layers) {
				if (strcmp(layer_name, layer_properties.layerName) == 0) {
					layer_found = true;
					break;
				}
			}

			//모든 레이어가 있지 않음
			if (!layer_found)
				return false;
		}

		//모든 레이어가 있음
		return true;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
		VkDebugUtilsMessageTypeFlagsEXT message_type,
		const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
		void* puser_data) {
		std::cerr << "validation layer: " << p_callback_data->pMessage << std::endl;

		return VK_FALSE;
	}

	void pick_physical_device() {
		uint32_t device_cnt = 0;

		vkEnumeratePhysicalDevices(instance, &device_cnt, nullptr);

		//사용자의 컴퓨터에서 Vulkan을 지원하는 기기가 없을 시 더 이상 진행할 의미가 없으므로 throw
		if (device_cnt == 0) {
			throw std::runtime_error("failed to find GPUs with Vulkan supports!");
		}

		std::vector<VkPhysicalDevice> devices(device_cnt);
		vkEnumeratePhysicalDevices(instance, &device_cnt, devices.data());

		for (const auto& device : devices) {
			if (is_device_suitable(device)) {
				physical_device = device;

				break;
			}
		}

		if (physical_device == VK_NULL_HANDLE)
			throw std::runtime_error("failed to find a suitable GPU!");
	}

	//수행하고자 하는 작업이 적합한지 확인
	bool is_device_suitable(VkPhysicalDevice device) {
		queue_family_indices indices = find_queue_families(device);

		return indices.is_complete();
	}

	queue_family_indices find_queue_families (VkPhysicalDevice device) {
		queue_family_indices indices;

		uint32_t queue_family_cnt = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_cnt, nullptr);

		std::vector<VkQueueFamilyProperties> queue_familes(queue_family_cnt);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_cnt, queue_familes.data());

		int i = 0;

		for (const auto& queue_family : queue_familes) {
			if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.graphics_family = 1;

			if (indices.is_complete())
				break;

			i++;
		}

		return indices;
	}

	void create_logical_device() {
		queue_family_indices indices = find_queue_families(physical_device);
		VkDeviceQueueCreateInfo queue_create_info{};
		VkPhysicalDeviceFeatures device_feature{};
		VkDeviceCreateInfo create_info{};
		float queue_priority = 1.0f;

		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = indices.graphics_family.value();
		queue_create_info.queueCount = 1;
		queue_create_info.pQueuePriorities = &queue_priority;

		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.pQueueCreateInfos = &queue_create_info;
		create_info.queueCreateInfoCount = 1;

		create_info.pEnabledFeatures = &device_feature;

		create_info.enabledExtensionCount = 0;

		if (enable_validation_layers) {
			create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
			create_info.ppEnabledLayerNames = validation_layers.data();
		}
		else {
			create_info.enabledLayerCount = 0;
		}

		//매개변수는 인터페이스할 물리적 장치, 방금 지정한 대기열 및 정보 선택적 할당 콜백 포인터, 논리적 장치 핸들을 저장할 변수에 대한 포인터
		if (vkCreateDevice(physical_device, &create_info, nullptr, &device) != VK_SUCCESS)
			throw std::runtime_error("failed to create logical device!");

		//매개변수는 논리적 장치, 큐 패밀리, 큐 인덱스 및 큐 핸들을 저장할 매개변수 포인터(단일 큐만 생성하므로 인덱스는 간단하게 0으로)
		vkGetDeviceQueue(device, indices.graphics_family.value(), 0, &graphics_queue);
	}
};

int main() {
	Application app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}