ROOT_STANDARD_LIBRARY_PACKAGE(ROOTLLRWgpu
  HEADERS
    ROOT/LLRWgpu.hxx
  SOURCES
    src/LLRWgpu.cxx
  LIBRARIES
    # Freetype::Freetype
    # RVulkan
    # glfw
  DEPENDENCIES
    ROOTLLR
)
