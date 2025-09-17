add_library(RVulkan SHARED
                    src/RVulkan.cxx
                    # src/RVulkanImpl.cxx
                   # src/RHistProxy.cxx
            )

target_include_directories(RVulkan PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>)

install(TARGETS RVulkan EXPORT ROOTExports)
set_property(GLOBAL APPEND PROPERTY ROOT_EXPORTED_TARGETS RVulkan)

find_package(Vulkan REQUIRED)
# TODO: find out if we can use the target for GLFW (add a FindGLFW)
# find_package(glfw3 REQUIRED)
target_link_libraries(RVulkan PRIVATE Vulkan::Vulkan glfw)
