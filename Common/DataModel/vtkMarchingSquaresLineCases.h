// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#ifndef vtkMarchingSquaresLineCases_h
#define vtkMarchingSquaresLineCases_h
//
// Marching squares cases for generating isolines.
//
#include "vtkCommonDataModelModule.h" // For export macro
#include "vtkDeprecation.h"
#include "vtkSystemIncludes.h"

VTK_ABI_NAMESPACE_BEGIN
struct VTK_DEPRECATED_IN_9_7_0(
  "Use vtkMarchingCellsContourCases") VTKCOMMONDATAMODEL_EXPORT vtkMarchingSquaresLineCases
{
  int edges[5];
  VTK_DEPRECATED_IN_9_7_0("Use vtkMarchingCellsContourCases::GetQuadCases()")
  static vtkMarchingSquaresLineCases* GetCases();
};

VTK_ABI_NAMESPACE_END
#endif
// VTK-HeaderTest-Exclude: vtkMarchingSquaresLineCases.h
