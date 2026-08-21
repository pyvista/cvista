// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#ifndef vtkMarchingCubesTriangleCases_h
#define vtkMarchingCubesTriangleCases_h
//
// marching cubes case table for generating isosurfaces
//
#include "vtkCommonDataModelModule.h" // For export macro
#include "vtkDeprecation.h"
#include "vtkSystemIncludes.h"

VTK_ABI_NAMESPACE_BEGIN
struct VTK_DEPRECATED_IN_9_7_0(
  "Use vtkMarchingCellsContourCases") VTKCOMMONDATAMODEL_EXPORT vtkMarchingCubesTriangleCases
{
  int edges[16];

  VTK_DEPRECATED_IN_9_7_0("Use vtkMarchingCellsContourCases::GetHexahedronCases()")
  static vtkMarchingCubesTriangleCases* GetCases();
};

VTK_ABI_NAMESPACE_END
#endif
// VTK-HeaderTest-Exclude: vtkMarchingCubesTriangleCases.h
