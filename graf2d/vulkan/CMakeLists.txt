find_package(glfw3 3 REQUIRED)

# @Temporary: remove this
add_compile_options(-Wno-error)
add_subdirectory(rvulkan)

ROOT_STANDARD_LIBRARY_PACKAGE(GVulkan
  HEADERS
    TGVulkan.h
  SOURCES
    src/TGVulkan.cxx
  LIBRARIES
    Freetype::Freetype
  DEPENDENCIES
    Core
)

# Don't install the private RVulkan lib
target_link_libraries(GVulkan PRIVATE RVulkan glfw)
