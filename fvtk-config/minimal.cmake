# fvtk MINIMAL profile — the lightest possible PyVista-parity VTK.
#
# Deny-by-default module policy (only PyVista's measured closure, ~84 modules
# vs ~160 for BUILD_ALL) + the production-quality knobs from the CoDim config
# we want to keep (KITS, Python wrapping, EGL+OSMesa+X rendering, logging-on).
#
# Self-contained: does NOT include _base.cmake / _modules.cmake (those turn
# BUILD_ALL_MODULES ON). LTO is OFF here for fast local iteration; flip it ON
# for production wheels.
#
# Configure with:  cmake -S <src> -B <build> -C fvtk-config/minimal.cmake ...

# --- module policy: deny-by-default, enable only PyVista's closure -----------
include("${CMAKE_CURRENT_LIST_DIR}/_modules_minimal.cmake")

# --- build hygiene -----------------------------------------------------------
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
set(VTK_BUILD_TESTING OFF CACHE BOOL "")
set(VTK_BUILD_DOCUMENTATION OFF CACHE BOOL "")
set(VTK_BUILD_EXAMPLES OFF CACHE BOOL "")
set(VTK_DEBUG_LEAKS OFF CACHE BOOL "")
set(VTK_ENABLE_REMOTE_MODULES OFF CACHE BOOL "")

# LTO OFF for fast validation builds. Production: set ON (~2-3x slower build).
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)

# --- production rendering / runtime flags (from CoDim _base.cmake) -----------
set(VTK_ENABLE_KITS ON CACHE BOOL "")            # ~modules -> ~kits: faster startup, smaller
set(VTK_LEGACY_REMOVE ON CACHE BOOL "")
set(VTK_REPORT_OPENGL_ERRORS OFF CACHE BOOL "")
set(VTK_ENABLE_LOGGING ON CACHE BOOL "")         # pyvista pv.vtk_verbosity() needs it ON
set(VTK_OPENGL_ENABLE_STREAM_ANNOTATIONS OFF CACHE BOOL "")
set(VTK_DISPATCH_SCALED_SOA_ARRAYS ON CACHE BOOL "")
set(VTK_DISPATCH_SOA_ARRAYS ON CACHE BOOL "")
set(VTK_PYTHON_FULL_THREADSAFE ON CACHE BOOL "")
set(VTK_NO_PYTHON_THREADS OFF CACHE BOOL "")

# --- Python wheel ------------------------------------------------------------
set(VTK_WHEEL_BUILD ON CACHE BOOL "")
set(VTK_INSTALL_SDK ON CACHE BOOL "")
set(VTK_WRAP_PYTHON YES CACHE BOOL "")
# .pyi type stubs are dev-only IDE hints, not needed at runtime. OFF for the
# lightest build; flip ON if shipping editor type-completion parity matters.
set(VTK_BUILD_PYI_FILES OFF CACHE BOOL "")
set(VTK_USE_PCH OFF CACHE BOOL "")
set(VTK_RELOCATABLE_INSTALL ON CACHE BOOL "")
# Serialization OFF: this nightly has no SerializationManager/WebAssembly
# session modules, so VTK_WRAP_SERIALIZATION=ON makes the resolver require
# them and configure fails. (Same fix as fast.cmake.)
set(VTK_WRAP_SERIALIZATION OFF CACHE BOOL "")

# --- rendering backends: X/GLX + EGL + OSMesa, matching the stock pyvista wheel
set(VTK_DEFAULT_RENDER_WINDOW_HEADLESS False CACHE BOOL "")
set(VTK_OPENGL_HAS_EGL True CACHE BOOL "")
set(VTK_OPENGL_HAS_OSMESA True CACHE BOOL "")
set(VTK_USE_COCOA False CACHE BOOL "")
set(VTK_USE_X True CACHE BOOL "")

# Drop-in for the stock `vtk` wheel: keep the import namespace, no dist suffix.
set(VTK_DIST_NAME_SUFFIX "" CACHE STRING "")
