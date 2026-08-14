// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
// VTK_DEPRECATED_IN_9_7_0()
#define VTK_DEPRECATION_LEVEL 0

#include "vtkMarchingSquaresLineCases.h"
#include "vtkMarchingCellsContourCases.h"

// Note: the following code is placed here to deal with cross-library
// symbol export and import on Microsoft compilers.
VTK_ABI_NAMESPACE_BEGIN
vtkMarchingSquaresLineCases* vtkMarchingSquaresLineCases::GetCases()
{
  // Since the old API returned a pointer to a struct,
  // we cast the new unified array back to the old struct type.
  return reinterpret_cast<vtkMarchingSquaresLineCases*>(
    const_cast<vtkMarchingCellsContourCases::QuadCase*>(
      vtkMarchingCellsContourCases::GetQuadCases()));
}
VTK_ABI_NAMESPACE_END
