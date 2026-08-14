// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkHierarchicalDataExtractDataSets
 * @brief   extract a number of datasets
 *
 * Legacy class. Use vtkExtractDataSets instead.
 *
 * @sa
 * vtkExtractDataSets
 */

#ifndef vtkHierarchicalDataExtractDataSets_h
#define vtkHierarchicalDataExtractDataSets_h

#include "vtkDeprecation.h" // for VTK_DEPRECATED_IN_9_7_0
#include "vtkExtractDataSets.h"
#include "vtkFiltersExtractionModule.h" // For export macro

VTK_ABI_NAMESPACE_BEGIN
struct vtkHierarchicalDataExtractDataSetsInternals;

class VTK_DEPRECATED_IN_9_7_0("Use vtkHierarchicalDataExtractDataSets")
  VTKFILTERSEXTRACTION_EXPORT vtkHierarchicalDataExtractDataSets : public vtkExtractDataSets
{
public:
  vtkTypeMacro(vtkHierarchicalDataExtractDataSets, vtkExtractDataSets);
  void PrintSelf(ostream& os, vtkIndent indent) override;
  static vtkHierarchicalDataExtractDataSets* New();

protected:
  vtkHierarchicalDataExtractDataSets();
  ~vtkHierarchicalDataExtractDataSets() override;

private:
  vtkHierarchicalDataExtractDataSets(const vtkHierarchicalDataExtractDataSets&) = delete;
  void operator=(const vtkHierarchicalDataExtractDataSets&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
