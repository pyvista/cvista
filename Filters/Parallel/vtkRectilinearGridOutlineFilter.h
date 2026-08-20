// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkRectilinearGridOutlineFilter
 * @brief   create wireframe outline for a rectilinear grid.
 *
 * vtkRectilinearGridOutlineFilter works in parallel.  There is no reason.
 * to use this filter if you are not breaking the processing into pieces.
 * With one piece you can simply use vtkOutlineFilter.  This filter
 * ignores internal edges when the extent is not the whole extent.
 */

#ifndef vtkRectilinearGridOutlineFilter_h
#define vtkRectilinearGridOutlineFilter_h

#include "vtkFiltersParallelModule.h" // For export macro
#include "vtkPolyDataAlgorithm.h"

VTK_ABI_NAMESPACE_BEGIN
class VTKFILTERSPARALLEL_EXPORT vtkRectilinearGridOutlineFilter : public vtkPolyDataAlgorithm
{
public:
  static vtkRectilinearGridOutlineFilter* New();
  vtkTypeMacro(vtkRectilinearGridOutlineFilter, vtkPolyDataAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /**
   * Set/get the desired precision for the output points.
   * vtkAlgorithm::DEFAULT_PRECISION - Output points have the same precision as
   *   the input coordinate arrays (the default).
   * vtkAlgorithm::SINGLE_PRECISION - Output points are single precision.
   * vtkAlgorithm::DOUBLE_PRECISION - Output points are double precision.
   */
  vtkSetMacro(OutputPointsPrecision, int);
  vtkGetMacro(OutputPointsPrecision, int);
  ///@}

protected:
  vtkRectilinearGridOutlineFilter() = default;
  ~vtkRectilinearGridOutlineFilter() override = default;
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;
  int FillInputPortInformation(int port, vtkInformation* info) override;

  int OutputPointsPrecision = DEFAULT_PRECISION;

private:
  vtkRectilinearGridOutlineFilter(const vtkRectilinearGridOutlineFilter&) = delete;
  void operator=(const vtkRectilinearGridOutlineFilter&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
