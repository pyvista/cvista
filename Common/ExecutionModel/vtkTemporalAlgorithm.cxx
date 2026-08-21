// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
// Single, exported instantiation of the vtkPolyDataAlgorithm specialization of
// vtkTemporalAlgorithm. Defining vtkTemporalAlgorithm_cxx turns the header's
// `extern template` declaration into this definition, so every other translation
// unit imports it instead of emitting its own weak copy (which collides under
// MSVC when cvista merges modules into shared kit DLLs -- see the header).
#define vtkTemporalAlgorithm_cxx
#include "vtkTemporalAlgorithm.h"

#include "vtkPolyDataAlgorithm.h"

VTK_ABI_NAMESPACE_BEGIN
template class VTKCOMMONEXECUTIONMODEL_EXPORT vtkTemporalAlgorithm<vtkPolyDataAlgorithm>;
VTK_ABI_NAMESPACE_END
