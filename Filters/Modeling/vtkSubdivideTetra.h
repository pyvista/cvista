// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkSubdivideTetra
 * @brief   subdivide one tetrahedron into twelve for every tetra
 *
 * This filter subdivides tetrahedra in an unstructured grid into twelve tetrahedra.
 */

#ifndef vtkSubdivideTetra_h
#define vtkSubdivideTetra_h

#include "vtkFiltersModelingModule.h" // For export macro
#include "vtkUnstructuredGridAlgorithm.h"

VTK_ABI_NAMESPACE_BEGIN
class VTKFILTERSMODELING_EXPORT vtkSubdivideTetra : public vtkUnstructuredGridAlgorithm
{
public:
  static vtkSubdivideTetra* New();
  vtkTypeMacro(vtkSubdivideTetra, vtkUnstructuredGridAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /**
   * Set/get the desired precision for the output points.
   * vtkAlgorithm::DEFAULT_PRECISION - Output points have the same precision as
   *   the input points (the default).
   * vtkAlgorithm::SINGLE_PRECISION - Output points are single precision.
   * vtkAlgorithm::DOUBLE_PRECISION - Output points are double precision.
   */
  vtkSetMacro(OutputPointsPrecision, int);
  vtkGetMacro(OutputPointsPrecision, int);
  ///@}

protected:
  vtkSubdivideTetra();
  ~vtkSubdivideTetra() override = default;

  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;

  int OutputPointsPrecision = DEFAULT_PRECISION;

private:
  vtkSubdivideTetra(const vtkSubdivideTetra&) = delete;
  void operator=(const vtkSubdivideTetra&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
