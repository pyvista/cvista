# fvtk: build mimalloc as a single static object (libvtkmimalloc.a) from the
# upstream single-source amalgamation (src/static.c, which #includes every other
# src/*.c). This is the form mimalloc documents for an in-tree STATIC override.
#
# We compile WITHOUT MI_MALLOC_OVERRIDE: with the override macro undefined,
# src/alloc-override.c is a no-op (it is `#if defined(MI_MALLOC_OVERRIDE)`), so
# this object exports NO malloc/free/operator-new symbols -- only the internal
# `mi_*` C API. The global operator new/delete override that actually routes
# VTK's C++ allocations lives in fvtk's own TU (Common/Core/vtkFVTKAllocator.cxx)
# and forwards to mi_malloc_aligned/mi_free. Nothing here ever interposes the
# host CPython allocator (enforced by ci/check-no-alloc-exports.sh).

set(sources
  src/static.c)

# The public mimalloc API header. static.c pulls in the rest of src/*.c by
# relative path at compile time, so only static.c is a compiled source.
set(headers
  include/mimalloc.h)

vtk_module_add_module(VTK::mimalloc
  SOURCES ${sources}
  HEADERS ${headers}
  HEADERS_SUBDIR "vtkmimalloc")

# Keep the mi_* C API fvtk-internal. fvtk sets CMAKE_CXX_VISIBILITY_PRESET=hidden
# globally but NOT the C preset, and under MI_STATIC_LIB mimalloc's mi_decl_export
# is empty (no __attribute__((visibility))) -- so without this the mi_* C symbols
# would land in the kit .so's .dynsym at default visibility. Force hidden so the
# whole allocator (and the operator new/delete TU that calls it) stays bound
# inside fvtk's own .so's and nothing extra is exported. Belt-and-suspenders to
# the nm -D guard (which checks malloc/free/operator new specifically).
_vtk_module_real_target(_fvtk_mi_target VTK::mimalloc)
set_target_properties("${_fvtk_mi_target}"
  PROPERTIES
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
unset(_fvtk_mi_target)

# static.c does `#include "mimalloc.h"` / `#include "mimalloc/internal.h"` and
# pulls in src/*.c by relative path, so both include/ and src/ must be on the
# include path. include/ is PUBLIC so dependents (vtkCommonCore's allocator TU)
# resolve `<mimalloc.h>`; src/ is PRIVATE (only static.c's relative includes).
vtk_module_include(VTK::mimalloc
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Static, release-quality allocator with NO malloc/operator-new interposition:
#   MI_STATIC_LIB    -> static-archive symbol linkage (matches our libvtkmimalloc.a)
#   MI_BUILD_RELEASE -> MI_DEBUG=0, no assertions/padding (release allocator)
# We deliberately DO NOT define MI_MALLOC_OVERRIDE (no malloc/free/new override)
# and DO NOT define MI_SHARED_LIB (we are not a DSO).
vtk_module_definitions(VTK::mimalloc
  PRIVATE
    "MI_STATIC_LIB"
    "MI_BUILD_RELEASE")

# mimalloc's unix primitive layer (src/prim/unix/prim.c) needs pthreads, dl, and
# librt (mmap/clock). Windows is not built here (the lever is gated off on MSVC).
if (NOT WIN32)
  vtk_module_find_package(PACKAGE Threads)
  vtk_module_link(VTK::mimalloc PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
  if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    vtk_module_link(VTK::mimalloc PRIVATE rt)
  endif ()
endif ()
