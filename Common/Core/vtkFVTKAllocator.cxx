// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
//
// fvtk (pyvista/fvtk) addition -- NOT part of upstream VTK.
//
// Global C++ operator new/delete override that routes fvtk's own heap
// allocations to the vendored, statically-linked mimalloc
// (ThirdParty/mimalloc -> libvtkmimalloc.a). This is the ONLY allocator hook in
// fvtk; there is no LD_PRELOAD, no mimalloc malloc/free interposition, and no
// mimalloc redirect/preload mechanism.
//
// WHY THIS IS SAFE (no host interposition / "just works" on pip install):
//   * fvtk is built with CMAKE_CXX_VISIBILITY_PRESET=hidden +
//     CMAKE_VISIBILITY_INLINES_HIDDEN and -fno-semantic-interposition. These
//     operator new/delete definitions carry NO export macro, so they are emitted
//     as HIDDEN symbols. They satisfy fvtk's own intra-.so operator new/delete
//     references but are NOT placed in the dynamic symbol table, so they cannot
//     interpose the host CPython allocator, libstdc++, or any other extension
//     module. The nm -D guard (ci/check-no-alloc-exports.sh) asserts this on the
//     built artifacts and fails the build if any malloc/free/operator new/delete
//     symbol is ever exported.
//   * mimalloc itself is compiled WITHOUT MI_MALLOC_OVERRIDE, so libvtkmimalloc.a
//     exports no malloc/free/operator new -- only the internal mi_* C API that
//     this TU calls.
//
// WHY THIS IS BYTE-EXACT: an allocator only changes the ADDRESSES returned by
// new/malloc; it never changes any value, count, ordering, or layout that fvtk
// computes. The bitexact (maxULP=0), renderexact, and pyvista parity gates are
// the proof.
//
// SCOPE: this TU is compiled into vtkCommonCore, so the override binds within
// the vtkCommon kit .so (where VTK's array/object/point/cell allocation lives --
// the hot allocation paths for the threaded filters). Extending the override to
// the other kit .so files is a documented follow-up (see the PR body).

// Windows is deliberately out of scope: no fvtk Windows wheel is shipped, and a
// global operator-new override interacts with the MSVC CRT differently (it would
// need mimalloc-redirect.dll, which is exactly the preload-style mechanism the
// design forbids). The CMake gate in fvtk-config/minimal.cmake already forces
// FVTK_MIMALLOC OFF on Windows so this TU is never compiled there; this #if is a
// second belt so an accidental Windows compile is a clean no-op rather than a
// surprising CRT override.
#if defined(_WIN32)

// Intentionally empty on Windows.

#else

#include <cstddef>
#include <new>

#include <mimalloc.h>

// All of these are intentionally NOT marked with any VTK*_EXPORT macro: under
// the build's hidden default visibility they resolve fvtk-internal references
// only and are never exported. mimalloc's mi_malloc_aligned honors C++'s
// fundamental alignment guarantee; we pass alignof(std::max_align_t) for the
// unaligned overloads (matching the platform malloc contract) and the requested
// alignment for the aligned (C++17) overloads.

namespace
{
constexpr std::size_t kFvtkMaxAlign = alignof(std::max_align_t);

inline void* fvtk_mi_alloc(std::size_t size, std::size_t align)
{
  // mimalloc requires a non-zero allocation to always return a unique pointer;
  // mi_malloc_aligned(0, ...) already returns a valid unique pointer, matching
  // the operator new(0) requirement.
  return mi_malloc_aligned(size, align);
}
} // namespace

// ---- throwing operator new ----------------------------------------------------
void* operator new(std::size_t size)
{
  void* p = fvtk_mi_alloc(size, kFvtkMaxAlign);
  if (!p)
  {
    throw std::bad_alloc();
  }
  return p;
}

void* operator new[](std::size_t size)
{
  void* p = fvtk_mi_alloc(size, kFvtkMaxAlign);
  if (!p)
  {
    throw std::bad_alloc();
  }
  return p;
}

// ---- nothrow operator new -----------------------------------------------------
void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
  return fvtk_mi_alloc(size, kFvtkMaxAlign);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
  return fvtk_mi_alloc(size, kFvtkMaxAlign);
}

// ---- aligned operator new (C++17) ---------------------------------------------
void* operator new(std::size_t size, std::align_val_t al)
{
  void* p = fvtk_mi_alloc(size, static_cast<std::size_t>(al));
  if (!p)
  {
    throw std::bad_alloc();
  }
  return p;
}

void* operator new[](std::size_t size, std::align_val_t al)
{
  void* p = fvtk_mi_alloc(size, static_cast<std::size_t>(al));
  if (!p)
  {
    throw std::bad_alloc();
  }
  return p;
}

void* operator new(std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept
{
  return fvtk_mi_alloc(size, static_cast<std::size_t>(al));
}

void* operator new[](std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept
{
  return fvtk_mi_alloc(size, static_cast<std::size_t>(al));
}

// ---- operator delete ----------------------------------------------------------
// mi_free safely handles a null pointer and pointers from any mi_malloc_aligned
// alignment, so every delete variant routes through it. Sized/aligned deletes
// pass the same pointer; mimalloc tracks the block internally.
void operator delete(void* p) noexcept
{
  mi_free(p);
}

void operator delete[](void* p) noexcept
{
  mi_free(p);
}

void operator delete(void* p, const std::nothrow_t&) noexcept
{
  mi_free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept
{
  mi_free(p);
}

// sized delete (C++14)
void operator delete(void* p, std::size_t) noexcept
{
  mi_free(p);
}

void operator delete[](void* p, std::size_t) noexcept
{
  mi_free(p);
}

// aligned delete (C++17)
void operator delete(void* p, std::align_val_t) noexcept
{
  mi_free(p);
}

void operator delete[](void* p, std::align_val_t) noexcept
{
  mi_free(p);
}

void operator delete(void* p, std::size_t, std::align_val_t) noexcept
{
  mi_free(p);
}

void operator delete[](void* p, std::size_t, std::align_val_t) noexcept
{
  mi_free(p);
}

void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept
{
  mi_free(p);
}

void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept
{
  mi_free(p);
}

#endif // !_WIN32
