find_package(glfw3 3 REQUIRED)

add_compile_options(-Wno-error) # TEMP
add_subdirectory(rvulkan)

ROOT_STANDARD_LIBRARY_PACKAGE(GVulkan
  HEADERS
    TGVulkan.h
  SOURCES
    src/TGVulkan.cxx
  LIBRARIES
    ${Vulkan_LIBRARIES}
    glfw #${GLFW_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    RVulkan
    ${CMAKE_THREAD_LIBS_INIT}
  DEPENDENCIES
    Core
    #MathCore
  #INSTALL_OPTIONS
    #FILTER "inc/X11/"
    #FILTER "X11$"
)

target_include_directories(GVulkan PRIVATE ${Vulkan_INCLUDE_DIR})
target_include_directories(GVulkan PRIVATE ${FREETYPE_INCLUDE_DIR})
target_include_directories(GVulkan PRIVATE "${PROJECT_SOURCE_DIR}/rvulkan/inc")
