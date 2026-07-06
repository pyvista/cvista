// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkStatisticalOutlierRemoval.h"

#include "vtkAbstractPointLocator.h"
#include "vtkArrayDispatch.h"
#include "vtkArrayDispatchDataSetArrayList.h"
#include "vtkDataArrayRange.h"
#include "vtkIdList.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPointSet.h"
#include "vtkPoints.h"
#include "vtkSMPThreadLocalObject.h"
#include "vtkSMPTools.h"
#include "vtkStaticPointLocator.h"

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkStatisticalOutlierRemoval);
vtkCxxSetObjectMacro(vtkStatisticalOutlierRemoval, Locator, vtkAbstractPointLocator);

//------------------------------------------------------------------------------
// Helper classes to support efficient computing, and threaded execution.
namespace
{

//------------------------------------------------------------------------------
// The threaded core of the algorithm (first pass)
template <typename TArray>
struct ComputeMeanDistanceFunctor
{
  TArray* Points;
  vtkAbstractPointLocator* Locator;
  int SampleSize;
  float* Distance;
  vtkIdType NumPts;
  double Mean;

  // Don't want to allocate working arrays on every thread invocation. Thread local
  // storage lots of new/delete.
  vtkSMPThreadLocalObject<vtkIdList> PIds;

  ComputeMeanDistanceFunctor(
    TArray* points, vtkAbstractPointLocator* loc, int size, float* d, vtkIdType numPts)
    : Points(points)
    , Locator(loc)
    , SampleSize(size)
    , Distance(d)
    , NumPts(numPts)
    , Mean(0.0)
  {
  }

  // Just allocate a little bit of memory to get started.
  void Initialize()
  {
    vtkIdList*& pIds = this->PIds.Local();
    pIds->Allocate(128); // allocate some memory
  }

  // Compute the average distance to the SampleSize nearest neighbors for each
  // point, storing it in Distance[]. This per-point result is independent of the
  // thread partitioning; the global mean is reduced deterministically in Reduce().
  void operator()(vtkIdType ptId, vtkIdType endPtId)
  {
    auto points = vtk::DataArrayTupleRange<3>(this->Points);
    auto px = points.begin() + ptId;
    double x[3], y[3];
    vtkIdList*& pIds = this->PIds.Local();

    for (; ptId < endPtId; ++ptId, ++px)
    {
      px->GetTuple(x);

      // The method FindClosestNPoints will include the current point, so
      // we increase the sample size by one.
      this->Locator->FindClosestNPoints(this->SampleSize + 1, x, pIds);
      vtkIdType numPts = pIds->GetNumberOfIds();

      double sum = 0.0;
      vtkIdType nei;
      for (int sample = 0; sample < numPts; ++sample)
      {
        nei = pIds->GetId(sample);
        if (nei != ptId) // exclude ourselves
        {
          auto py = points[nei];
          py.GetTuple(y);
          sum += sqrt(vtkMath::Distance2BetweenPoints(x, y));
        }
      } // sum the lengths of all samples excluding current point

      // Average the lengths; again exclude ourselves
      if (numPts > 0)
      {
        this->Distance[ptId] = sum / static_cast<double>(numPts - 1);
      }
      else // ignore if no points are found, something bad has happened
      {
        this->Distance[ptId] = VTK_FLOAT_MAX; // the effect is to eliminate it
      }
    }
  }

  // Compute the mean deterministically. The threaded operator() above fills the
  // per-point Distance[] array, which is byte-identical regardless of how the
  // point range was partitioned across threads (each point's neighbor search and
  // distance sum is independent of the partitioning). Sum those distances here in
  // a single serial, input-point-ordered pass. This intentionally replaces the
  // former vtkSMPThreadLocal partial-sum composite, whose result depended on the
  // number/order of partitions (FP addition is not associative). A ULP-level
  // difference in Mean shifts the mean+k*sigma keep/drop threshold and can flip a
  // borderline point, changing the SURVIVOR COUNT between serial and parallel
  // runs -- a count-sacred violation. A fixed-order sum is partition-independent
  // and byte-identical to the serial result. Runs once, after the parallel For.
  void Reduce()
  {
    double mean = 0.0;
    vtkIdType count = 0;
    for (vtkIdType ptId = 0; ptId < this->NumPts; ++ptId)
    {
      if (this->Distance[ptId] < VTK_FLOAT_MAX)
      {
        mean += this->Distance[ptId];
        count++;
      }
    }

    count = (count < 1 ? 1 : count);
    this->Mean = mean / static_cast<double>(count);
  }
}; // ComputeMeanDistanceFunctor

struct ComputeMeanDistanceWorker
{
  template <typename TArray>
  void operator()(
    TArray* points, vtkStatisticalOutlierRemoval* self, float* distances, double& mean)
  {
    const vtkIdType numTuples = points->GetNumberOfTuples();
    ComputeMeanDistanceFunctor<TArray> meanDist(
      points, self->GetLocator(), self->GetSampleSize(), distances, numTuples);
    vtkSMPTools::For(0, numTuples, meanDist);
    mean = meanDist.Mean;
  }
};

//------------------------------------------------------------------------------
// Now that the mean is known, compute the standard deviation.
//
// This is computed with a single serial, input-point-ordered reduction rather
// than a threaded vtkSMPThreadLocal composite. The per-point work here is
// trivial (one subtract + multiply over the already-computed Distances[]), so
// threading it bought nothing, while the partial-sum composite made the result
// depend on the thread/partition count (FP addition is not associative). A
// ULP-level difference in the standard deviation shifts the mean+k*sigma
// keep/drop threshold and can flip a borderline point, changing the SURVIVOR
// COUNT between serial and parallel runs. A fixed-order sum is partition-
// independent and byte-identical to the serial result.
struct ComputeStdDev
{
  static void Execute(vtkIdType numPts, float* distances, double mean, double& sigma)
  {
    double s = 0.0;
    vtkIdType count = 0;
    for (vtkIdType ptId = 0; ptId < numPts; ++ptId)
    {
      const float d = distances[ptId];
      if (d < VTK_FLOAT_MAX)
      {
        s += (mean - d) * (mean - d);
        count++;
      }
    }

    sigma = sqrt(s / static_cast<double>(count));
  }

}; // ComputeStdDev

//------------------------------------------------------------------------------
// Statistics are computed, now filter the points
struct RemoveOutliers
{
  double Mean;
  double Sigma;
  float* Distances;
  vtkIdType* PointMap;

  RemoveOutliers(double mean, double sigma, float* distances, vtkIdType* map)
    : Mean(mean)
    , Sigma(sigma)
    , Distances(distances)
    , PointMap(map)
  {
  }

  void operator()(vtkIdType ptId, vtkIdType endPtId)
  {
    vtkIdType* map = this->PointMap + ptId;
    float* d = this->Distances + ptId;
    double mean = this->Mean, sigma = this->Sigma;

    for (; ptId < endPtId; ++ptId)
    {
      *map++ = (fabs(*d++ - mean) <= sigma ? 1 : -1);
    }
  }

  static void Execute(vtkIdType numPts, float* distances, double mean, double sigma, vtkIdType* map)
  {
    RemoveOutliers remove(mean, sigma, distances, map);
    vtkSMPTools::For(0, numPts, remove);
  }

}; // RemoveOutliers

} // anonymous namespace

//================= Begin class proper =======================================
//------------------------------------------------------------------------------
vtkStatisticalOutlierRemoval::vtkStatisticalOutlierRemoval()
{
  this->SampleSize = 25;
  this->StandardDeviationFactor = 1.0;
  this->Locator = vtkStaticPointLocator::New();

  this->ComputedMean = 0.0;
  this->ComputedStandardDeviation = 0.0;
}

//------------------------------------------------------------------------------
vtkStatisticalOutlierRemoval::~vtkStatisticalOutlierRemoval()
{
  this->SetLocator(nullptr);
}

//------------------------------------------------------------------------------
// Traverse all the input points and gather statistics about average distance
// between them, and the standard deviation of variation. Then filter points
// within a specified deviation from the mean.
int vtkStatisticalOutlierRemoval::FilterPoints(vtkPointSet* input)
{
  // Perform the point removal
  // Start by building the locator
  if (!this->Locator)
  {
    vtkErrorMacro(<< "Point locator required\n");
    return 0;
  }
  this->Locator->SetDataSet(input);
  this->Locator->BuildLocator();

  // Compute statistics across the point cloud. Start my computing
  // mean distance to N closest neighbors.
  vtkIdType numPts = input->GetNumberOfPoints();
  float* dist = new float[numPts];
  double mean = 0.0, sigma = 0.0;
  ComputeMeanDistanceWorker worker;
  if (!vtkArrayDispatch::DispatchByArray<vtkArrayDispatch::PointArrays>::Execute(
        input->GetPoints()->GetData(), worker, this, dist, mean))
  {
    worker(input->GetPoints()->GetData(), this, dist, mean);
  }

  // At this point the mean distance for each point, and across the point
  // cloud is known. Now compute global standard deviation.
  ComputeStdDev::Execute(numPts, dist, mean, sigma);

  // Finally filter the points based on specified deviation range.
  RemoveOutliers::Execute(
    numPts, dist, mean, this->StandardDeviationFactor * sigma, this->PointMap);

  // Assign derived variable
  this->ComputedMean = mean;
  this->ComputedStandardDeviation = sigma;

  // Clean up
  delete[] dist;

  return 1;
}

//------------------------------------------------------------------------------
void vtkStatisticalOutlierRemoval::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Sample Size: " << this->SampleSize << "\n";
  os << indent << "Standard Deviation Factor: " << this->StandardDeviationFactor << "\n";
  os << indent << "Locator: " << this->Locator << "\n";

  os << indent << "Computed Mean: " << this->ComputedMean << "\n";
  os << indent << "Computed Standard Deviation: " << this->ComputedStandardDeviation << "\n";
}
VTK_ABI_NAMESPACE_END
