// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
// VTK_DEPRECATED_IN_9_7_0()
#define VTK_DEPRECATION_LEVEL 0
#include "vtkHierarchicalDataLevelFilter.h"

#include "vtkObjectFactory.h"

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkHierarchicalDataLevelFilter);

// Construct object with PointIds and CellIds on; and ids being generated
// as scalars.
vtkHierarchicalDataLevelFilter::vtkHierarchicalDataLevelFilter() = default;

vtkHierarchicalDataLevelFilter::~vtkHierarchicalDataLevelFilter() = default;

void vtkHierarchicalDataLevelFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
VTK_ABI_NAMESPACE_END
