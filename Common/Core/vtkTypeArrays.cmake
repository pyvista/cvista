# This file generates arrays specialization subclasses for fixed types,
# like `vtkConstantTypeFloat32Array` or `vtkAffineTypeInt64Array`.
#
# Generated classes are not templated thus they can be wrapped.

include(vtkTypeLists)

# Configure `.in` class files depending on the requested backend
# and the concrete c++ type.
macro(_generate_array_specialization array_prefix vtk_type concrete_type deprecation)
  # used inside .in files
  set(VTK_TYPE_NAME "${vtk_type}")
  set(CONCRETE_TYPE "${concrete_type}")
  set(VTK_DEPRECATION "${deprecation}")

  set(_className "vtk${array_prefix}${VTK_TYPE_NAME}Array")

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/vtk${array_prefix}TypedArray.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${_className}.h"
    @ONLY)

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/vtk${array_prefix}TypedArray.cxx.inc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${_className}.cxx.inc"
    @ONLY)

  # append generated header to current module headers
  list(APPEND headers
    "${CMAKE_CURRENT_BINARY_DIR}/${_className}.h")

  # append generated source to the bulk instantiation of concrete_type
  if (type MATCHES "^vtkType")
    # String starts with "vtkType"
    vtk_get_fixed_size_type_mapping("${concrete_type}" numeric_type)
    string(REPLACE " " "_" _suffix "${numeric_type}")
  else ()
    string(REPLACE " " "_" _suffix "${concrete_type}")
  endif ()
  if (CVISTA_SPLIT_BULK_INSTANTIATE)
    # cvista split mode: compile each generated specialization (e.g. the
    # vtkType*Array.cxx that define vtkTypeFloat32Array::New() etc.) as its own TU
    # directly, rather than #include-ing it into the per-type bulk TU. Without this
    # these New() definitions would be generated but never compiled -> undefined
    # references at link time. (9.7 renamed the include fragment to .cxx.inc; the
    # split path compiles the standalone .cxx TU, the bulk path #includes .cxx.inc.)
    list(APPEND sources
      "${CMAKE_CURRENT_BINARY_DIR}/${_className}.cxx")
    if (CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|AppleClang|Clang)$")
      set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/${_className}.cxx"
        PROPERTIES COMPILE_OPTIONS "-Wno-attributes"
                   COMPILE_DEFINITIONS "VTK_DEPRECATION_LEVEL=0")
    endif ()
  else ()
    list(APPEND "bulk_instantiation_sources_${_suffix}"
      "#include \"${_className}.cxx.inc\"")
  endif ()

  unset(VTK_DEPRECATION)
  unset(VTK_TYPE_NAME)
  unset(CONCRETE_TYPE)
  unset(_className)
endmacro()

# VTK_DEPRECATED_IN_9_6_0 to be removed later
foreach (array_prefix IN ITEMS Affine Composite Constant Indexed)
  foreach (type IN LISTS vtk_numeric_types)
    vtk_type_to_camel_case("${type}" cased_type)
    set(deprecation "VTK_DEPRECATED_IN_9_6_0(\"Use vtk${array_prefix}Type${cased_type}Array instead\")")
    _generate_array_specialization("${array_prefix}" "${cased_type}" "${type}" "${deprecation}")
  endforeach ()
endforeach ()

# VTK_DEPRECATED_IN_9_7_0 to be removed later
foreach (array_prefix IN ITEMS ScaledSOA StdFunction)
  foreach (type IN LISTS vtk_fixed_size_numeric_types)
    vtk_fixed_size_type_to_without_prefix("${type}" "vtk" without_vtk_prefix)
    set(deprecation "VTK_DEPRECATED_IN_9_7_0(\"Use vtk${array_prefix}Type${without_vtk_prefix}Array instead\")")
    _generate_array_specialization("${array_prefix}" "${without_vtk_prefix}" "${type}" "${deprecation}")
  endforeach ()
endforeach ()

# cvista: dead-family trim (CVISTA_DROP_DEAD_ARRAYS, default ON). The keep set is
# 9.7's implicit/AOS/SOA specialization families used by PyVista (ImageData/
# structured grids + the dispatcher); Strided is the dead family omitted by default.
# (9.7 dropped ScaledSOA/StdFunction from this list and added StructuredPoint.)
set(_cvista_specialization_prefixes Affine Composite Constant Indexed SOA StructuredPoint)
if (NOT CVISTA_DROP_DEAD_ARRAYS)
  list(APPEND _cvista_specialization_prefixes Strided)
endif ()
foreach (array_prefix IN LISTS _cvista_specialization_prefixes)
  foreach (type IN LISTS vtk_fixed_size_numeric_types)
    vtk_fixed_size_type_to_without_prefix("${type}" "vtk" without_vtk_prefix)
    _generate_array_specialization("${array_prefix}" "${without_vtk_prefix}" "${type}" "")
  endforeach ()
endforeach ()

function(vtk_type_native type ctype class)
  string(TOUPPER "${type}" type_upper)
  set("vtk_type_native_${type}" "
#if VTK_TYPE_${type_upper} == VTK_${ctype}
# include \"${class}Array.h\"
# define vtkTypeArrayBase ${class}Array
#endif
"
    PARENT_SCOPE)
endfunction()

function(vtk_type_native_choice type preferred_ctype preferred_class fallback_ctype fallback_class)
  string(TOUPPER "${type}" type_upper)
  set("vtk_type_native_${type}" "
#if VTK_TYPE_${type_upper} == VTK_${preferred_ctype}
# include \"${preferred_class}Array.h\"
# define vtkTypeArrayBase ${preferred_class}Array
#elif VTK_TYPE_${type_upper} == VTK_${fallback_ctype}
# include \"${fallback_class}Array.h\"
# define vtkTypeArrayBase ${fallback_class}Array
#endif
"
    PARENT_SCOPE)
endfunction()

# Configure data arrays for platform-independent fixed-size types.
# Match the type selection here to that in vtkType.h.
vtk_type_native(Int8 SIGNED_CHAR vtkSignedChar)
vtk_type_native(UInt8 UNSIGNED_CHAR vtkUnsignedChar)
vtk_type_native(Int16 SHORT vtkShort)
vtk_type_native(UInt16 UNSIGNED_SHORT vtkUnsignedShort)
vtk_type_native(Int32 INT vtkInt)
vtk_type_native(UInt32 UNSIGNED_INT vtkUnsignedInt)
vtk_type_native_choice(Int64 LONG vtkLong LONG_LONG vtkLongLong)
vtk_type_native_choice(UInt64 UNSIGNED_LONG vtkUnsignedLong UNSIGNED_LONG_LONG vtkUnsignedLongLong)
vtk_type_native(Float32 FLOAT vtkFloat)
vtk_type_native(Float64 DOUBLE vtkDouble)

foreach (type IN LISTS vtk_fixed_size_numeric_types)
  vtk_fixed_size_type_to_without_prefix("${type}" "vtkType" vtk_type)
  set(VTK_TYPE_NAME "${vtk_type}")
  set(VTK_TYPE_NATIVE "${vtk_type_native_${vtk_type}}")
  if (VTK_TYPE_NATIVE)
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/vtkAOSTypedArray.h.in"
      "${CMAKE_CURRENT_BINARY_DIR}/${type}Array.h"
      @ONLY)
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/vtkAOSTypedArray.cxx.inc.in"
      "${CMAKE_CURRENT_BINARY_DIR}/${type}Array.cxx.inc"
      @ONLY)
    # append generated header to current module headers
    list(APPEND headers
      "${CMAKE_CURRENT_BINARY_DIR}/${type}Array.h")
    # append generated source to the bulk instantiation of concrete_type
    vtk_get_fixed_size_type_mapping("${type}" numeric_type)
    string(REPLACE " " "_" _suffix "${numeric_type}")
    if (CVISTA_SPLIT_BULK_INSTANTIATE)
      # cvista split mode: compile the plain vtkType*Array.cxx (vtkTypeFloat64Array
      # etc., which define their New()/ctor) as its own TU instead of #include-ing
      # it into the per-type bulk TU (9.7 renamed the include fragment to .cxx.inc).
      list(APPEND sources
        "${CMAKE_CURRENT_BINARY_DIR}/${type}Array.cxx")
      if (CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|AppleClang|Clang)$")
        set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/${type}Array.cxx"
          PROPERTIES COMPILE_OPTIONS "-Wno-attributes"
                     COMPILE_DEFINITIONS "VTK_DEPRECATION_LEVEL=0")
      endif ()
    else ()
      list(APPEND "bulk_instantiation_sources_${_suffix}"
        "#include \"${type}Array.cxx.inc\"")
    endif ()
  endif ()
endforeach ()
