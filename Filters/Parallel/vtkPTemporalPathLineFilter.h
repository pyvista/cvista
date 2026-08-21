// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkPTemporalPathLineFilter
 * @brief   Parallel version of vtkTemporalPathLineFilter.
 *
 * vtkPTemporalPathLineFilter extends vtkTemporalPathLineFilter with support for:
 * - vtkPartitionedDataSet input (iterates over partitions, feeding all into
 *   the same trail map keyed by global particle ID)
 * - Parallel selection allgather (so all ranks know which particles are selected)
 *
 * When the input is a plain vtkDataSet, behavior is identical to the superclass
 * (with the addition of selection allgather in parallel).
 *
 * @sa
 * vtkTemporalPathLineFilter vtkParticleTracer
 */

#ifndef vtkPTemporalPathLineFilter_h
#define vtkPTemporalPathLineFilter_h

#include "vtkFiltersParallelModule.h" // For export macro
#include "vtkTemporalPathLineFilter.h"

VTK_ABI_NAMESPACE_BEGIN
class vtkMultiProcessController;
class vtkPartitionedDataSet;

class VTKFILTERSPARALLEL_EXPORT vtkPTemporalPathLineFilter : public vtkTemporalPathLineFilter
{
public:
  vtkTypeMacro(vtkPTemporalPathLineFilter, vtkTemporalPathLineFilter);
  static vtkPTemporalPathLineFilter* New();
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /**
   * Set/Get the controller to use for parallel operations. If not set,
   * the global controller is used.
   */
  virtual void SetController(vtkMultiProcessController*);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);
  ///@}

protected:
  vtkPTemporalPathLineFilter();
  ~vtkPTemporalPathLineFilter() override;

  int FillInputPortInformation(int port, vtkInformation* info) override;
  int FillOutputPortInformation(int port, vtkInformation* info) override;

  int Initialize(vtkInformation* request, vtkInformationVector** inputVector,
    vtkInformationVector* outputVector) override;
  int Execute(vtkInformation* request, vtkInformationVector** inputVector,
    vtkInformationVector* outputVector) override;
  int Finalize(vtkInformation* request, vtkInformationVector** inputVector,
    vtkInformationVector* outputVector) override;

private:
  vtkPTemporalPathLineFilter(const vtkPTemporalPathLineFilter&) = delete;
  void operator=(const vtkPTemporalPathLineFilter&) = delete;

  /**
   * Find the first non-null partition in a partitioned dataset.
   * Returns nullptr if all partitions are null or empty.
   */
  vtkDataSet* GetFirstNonEmptyPartition(vtkPartitionedDataSet* pds);

  /**
   * Build a local polydata containing allgathered selection IDs.
   * Returns nullptr if the input selection is null. Returns the
   * input unchanged on a single-process controller or when no ID
   * array is found. Otherwise returns a newly allocated polydata
   * whose point data holds the sorted, deduplicated union of
   * selection IDs from all ranks.
   */
  vtkSmartPointer<vtkDataSet> AllGatherSelection(vtkDataSet* selection);

  vtkMultiProcessController* Controller;
};

VTK_ABI_NAMESPACE_END
#endif // vtkPTemporalPathLineFilter_h
