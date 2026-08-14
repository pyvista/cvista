// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkNew.h"
#include "vtkPolyData.h"

static vtkNew<vtkPolyData> StaticInstance;

int TestGlobalStaticvtkObject(int, char*[])
{
  return strcmp(StaticInstance->GetClassName(), "vtkPolyData") != 0;
}
