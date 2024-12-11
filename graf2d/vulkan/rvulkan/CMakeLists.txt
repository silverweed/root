add_library(RVulkan SHARED
                    src/RVulkan.cxx
                    # src/RVulkanImpl.cxx
                   # src/RHistProxy.cxx
            )

target_include_directories(RVulkan PRIVATE "inc")

install(TARGETS RVulkan EXPORT ROOTExports)
set_property(GLOBAL APPEND PROPERTY ROOT_EXPORTED_TARGETS RVulkan)

find_package(Vulkan REQUIRED)
target_link_libraries(RVulkan ${Vulkan_LIBRARIES} glfw)
