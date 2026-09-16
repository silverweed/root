# Low Level Renderer
# find_package(glfw3 3 REQUIRED)

# @Temporary: remove this
add_compile_options(-Wno-error)

ROOT_STANDARD_LIBRARY_PACKAGE(ROOTLLR
  HEADERS
    ROOT/LLRTypes.hxx
    ROOT/LLR.hxx
    ROOT/TLLRPadPainter.hxx
  SOURCES
    src/LLR.cxx
    src/TLLRPadPainter.cxx
  LIBRARIES
    # Freetype::Freetype
    # RVulkan
    # glfw
  DEPENDENCIES
    Core
    Gpad
)

# Don't install the private RVulkan lib
# target_link_libraries(GVulkan PRIVATE RVulkan glfw)

# target_include_directories(ROOTLLR PRIVATE ${CMAKE_SOURCE_DIR}/builtins/glad/inc)
