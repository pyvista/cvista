// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
// VTK_DEPRECATED_IN_9_7_0()
#define VTK_DEPRECATION_LEVEL 0

#include "vtkMarchingCubesTriangleCases.h"
#include "vtkMarchingCellsContourCases.h"

//=============================================================================
VTK_ABI_NAMESPACE_BEGIN
vtkMarchingCubesTriangleCases* vtkMarchingCubesTriangleCases::GetCases()
{
  // Since the old API returned a pointer to a struct,
  // we cast the new unified array back to the old struct type.
  return reinterpret_cast<vtkMarchingCubesTriangleCases*>(
    const_cast<vtkMarchingCellsContourCases::HexahedronCase*>(
      vtkMarchingCellsContourCases::GetHexahedronCases()));
}
VTK_ABI_NAMESPACE_END
