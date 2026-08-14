// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file    vtkOpenXRDefinitions.h
 * @brief   Defines types and enums for OpenXR.
 */

#ifndef vtkOpenXRDefinitions_h
#define vtkOpenXRDefinitions_h

#include "vtkRenderingOpenXRModule.h"

VTK_ABI_NAMESPACE_BEGIN

enum vtkOpenXRHand
{
  HAND_COUNT = 2,
};

enum vtkOpenXREye
{
  LEFT_EYE = 0,
  RIGHT_EYE = 1,
};

VTK_ABI_NAMESPACE_END

#endif
// VTK-HeaderTest-Exclude: vtkOpenXRDefinitions.h
