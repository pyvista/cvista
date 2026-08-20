# cvista build-lever re-hardening for VTK 9.7.0 (needed for the OPTIMIZED release wheel)
# Validation build runs with these OFF (parity-neutral). Re-enable + fix for release:
1. CVISTA_ABI3 (Py_LIMITED_API): 9.7's new vtkCollection sequence-protocol (PySequenceMethods,
   PyTuple_GET_SIZE/PyList_GET_SIZE macros in PyVTKObject.cxx/.h) is NOT abi3-safe.
   Fix: gate the sequence-protocol block under `#if !defined(Py_LIMITED_API)`; replace
   PyTuple_GET_SIZE->PyTuple_Size, PyList_GET_SIZE->PyList_Size, PyTuple_GET_ITEM->PyTuple_GetItem
   in the non-gated arg-passthrough path (PyVTKObject.cxx:582). G6's PyVTKObject.h:95 comment
   ("PySequenceMethods is part of the limited API") is WRONG — it is not.
2. CVISTA_DISPATCH_MINIMAL: trimmed list [double;float;vtkIdType;uchar] breaks 9.7's
   DispatchByValueType<char> in vtkDataWriter.cxx:1111 (empty Dispatch<NullType>).
   Fix: add `char` (and re-check signed char/short) to _cvista_disp_types in
   Common/Core/vtkCreateArrayDispatchArrayList.cmake, or guard 9.7's char-dispatch sites.
3. CVISTA_SOURCE_UNITY: 9.7 linear-cells anon-namespace symbols collide under 8-file unity
   (ParametricCoords in vtkVoxel/vtkPixel/..., OrderPoints in vtkOctreePointLocator/...).
   Fix: add the colliding .cxx to cvista-config/_source_unity_exclude.cmake.
