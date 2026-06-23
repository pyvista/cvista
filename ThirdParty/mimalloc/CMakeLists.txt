# fvtk: vendor mimalloc (microsoft/mimalloc, pinned v2.3.2) as an in-tree STATIC
# library (libvtkmimalloc.a). It backs fvtk's own global operator new/delete
# override (Common/Core/vtkFVTKAllocator.cxx); see that file and
# ThirdParty/mimalloc/vtkmimalloc/CMakeLists.txt for the no-interposition design.
#
# Enabled (VTK_MODULE_ENABLE_VTK_mimalloc=YES) only when the FVTK_MIMALLOC lever
# is ON (the default) on a non-Windows build; CommonCore's override TU references
# the mi_* API directly so the linker pulls these objects out of the static
# archive. Linux-first (the only shipped wheel); builds the same way on macOS;
# cleanly skipped on Windows (the lever is forced OFF there, no wheel shipped).
vtk_module_third_party_internal(
    LICENSE_FILES
      "vtkmimalloc/LICENSE"
    SPDX_LICENSE_IDENTIFIER
      "MIT"
    SPDX_COPYRIGHT_TEXT
      "Copyright (c) 2018-2025 Microsoft Corporation, Daan Leijen"
    SPDX_DOWNLOAD_LOCATION
      "git+https://github.com/microsoft/mimalloc.git@v2.3.2"
    VERSION
      "2.3.2"
    STANDARD_INCLUDE_DIRS)
