# Official stable libopus evaluation dependency, static and integrity pinned.
include(FetchContent)
set(OPUS_X86_MAY_HAVE_SSE4_1 ON CACHE BOOL "" FORCE)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_DRED OFF CACHE BOOL "" FORCE)
set(OPUS_OSCE OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_DEMO OFF CACHE BOOL "" FORCE)
FetchContent_Declare(fps_opus
  URL https://downloads.xiph.org/releases/opus/opus-1.6.1.tar.gz
  URL_HASH SHA256=6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(fps_opus)
# Upstream sets MSVC_CLANG_CL to the list "false;BOOL", which CMake treats
# as true. Remove its GCC-only source flag for actual MSVC; retain SIMD dispatch.
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  get_target_property(fps_opus_sources opus SOURCES)
  foreach(fps_opus_source IN LISTS fps_opus_sources)
    get_source_file_property(fps_opus_flags "${fps_opus_source}" DIRECTORY "${fps_opus_SOURCE_DIR}" COMPILE_FLAGS)
    if(fps_opus_flags AND fps_opus_flags MATCHES "-msse4.1")
      string(REPLACE "-msse4.1" "" fps_opus_flags "${fps_opus_flags}")
      set_source_files_properties("${fps_opus_source}" DIRECTORY "${fps_opus_SOURCE_DIR}" PROPERTIES COMPILE_FLAGS "${fps_opus_flags}")
    endif()
  endforeach()
endif()
add_library(fps_audio STATIC src/audio/audio_model.cpp src/audio/opus_codec.cpp)
target_include_directories(fps_audio PUBLIC src)
target_link_libraries(fps_audio PRIVATE Opus::opus)
fps_apply_warnings(fps_audio)
