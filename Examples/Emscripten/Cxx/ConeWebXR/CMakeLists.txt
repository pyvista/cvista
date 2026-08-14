cmake_minimum_required(VERSION 3.13)
project(ConeWebXR)

# -----------------------------------------------------------------------------
# EMSCRIPTEN only
# -----------------------------------------------------------------------------

if (NOT EMSCRIPTEN)
  message("Skipping example: This needs to run inside an Emscripten build environment")
  return ()
endif ()

# -----------------------------------------------------------------------------
# Handle VTK dependency
# -----------------------------------------------------------------------------

find_package(VTK
  COMPONENTS
    FiltersSources      # VTK pipeline
    InteractionStyle    # Mouse handling
    RenderingUI         # For window event processing
    RenderingWebXR      # For WebXR Rendering
)

if (NOT VTK_FOUND)
  message("Skipping example: ${VTK_NOT_FOUND_MESSAGE}")
  return ()
endif ()

# -----------------------------------------------------------------------------
# Compile example code
# -----------------------------------------------------------------------------

add_executable(ConeWebXR ConeWebXR.cxx)
target_link_libraries(ConeWebXR PRIVATE ${VTK_LIBRARIES})
target_link_options(ConeWebXR PRIVATE "SHELL:-sALLOW_MEMORY_GROWTH")

# -----------------------------------------------------------------------------
# Optimizations
# -----------------------------------------------------------------------------

set(emscripten_optimizations)
set(emscripten_debug_options)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Debug' as none was specified.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(cone_webxr_wasm_optimize "BEST")
  set(cone_webxr_wasm_debuginfo "NONE")
elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  set(cone_webxr_wasm_optimize "SMALLEST")
  set(cone_webxr_wasm_debuginfo "NONE")
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  set(cone_webxr_wasm_optimize "MORE")
  set(cone_webxr_wasm_debuginfo "PROFILE")
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(cone_webxr_wasm_optimize "NO_OPTIMIZATION")
  set(cone_webxr_wasm_debuginfo "DEBUG_NATIVE")
endif ()
set(cone_webxr_wasm_optimize_NO_OPTIMIZATION "-O0")
set(cone_webxr_wasm_optimize_LITTLE "-O1")
set(cone_webxr_wasm_optimize_MORE "-O2")
set(cone_webxr_wasm_optimize_BEST "-O3")
set(cone_webxr_wasm_optimize_SMALLEST "-Os")

if (DEFINED "cone_webxr_wasm_optimize_${cone_webxr_wasm_optimize}")
  list(APPEND emscripten_optimizations
    ${cone_webxr_wasm_optimize_${cone_webxr_wasm_optimize}})
  list(APPEND emscripten_link_options
    ${cone_webxr_wasm_optimize_${cone_webxr_wasm_optimize}_link})
else ()
  message (FATAL_ERROR "Unrecognized value for cone_webxr_wasm_optimize=${cone_webxr_wasm_optimize}")
endif ()

set(cone_webxr_wasm_debuginfo_NONE "-g0")
set(cone_webxr_wasm_debuginfo_READABLE_JS "-g1")
set(cone_webxr_wasm_debuginfo_PROFILE "-g2")
set(cone_webxr_wasm_debuginfo_DEBUG_NATIVE "-g3")
set(cone_webxr_wasm_debuginfo_DEBUG_NATIVE_link "-sASSERTIONS=1")
if (DEFINED "cone_webxr_wasm_debuginfo_${cone_webxr_wasm_debuginfo}")
  list(APPEND emscripten_debug_options
    ${cone_webxr_wasm_debuginfo_${cone_webxr_wasm_debuginfo}})
  list(APPEND emscripten_link_options
    ${cone_webxr_wasm_debuginfo_${cone_webxr_wasm_debuginfo}_link})
else ()
  message (FATAL_ERROR "Unrecognized value for cone_webxr_wasm_debuginfo=${cone_webxr_wasm_debuginfo}")
endif ()

target_compile_options(ConeWebXR
  PRIVATE
    ${emscripten_compile_options}
    ${emscripten_optimizations}
    ${emscripten_debug_options})
target_link_options(ConeWebXR
  PRIVATE
    ${emscripten_link_options}
    ${emscripten_optimizations}
    ${emscripten_debug_options})

# -----------------------------------------------------------------------------
# VTK modules initialization
# -----------------------------------------------------------------------------

vtk_module_autoinit(
  TARGETS  ConeWebXR
  MODULES  ${VTK_LIBRARIES}
)

# -----------------------------------------------------------------------------
# Copy HTML to build directory
# -----------------------------------------------------------------------------

add_custom_command(
  TARGET ConeWebXR
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/index.html"
      $<TARGET_FILE_DIR:ConeWebXR>
)
