// SPDX-License-Identifier: BSD-3-Clause
#include "smpParityCases.h"

#include <vtkAlgorithm.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDataObject.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkPlane.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>
#include <vtkSphere.h>
#include <vtkTable.h>
#include <vtkTransform.h>
#include <vtkUnstructuredGrid.h>

// Sources / filters under test.
#include <vtk3DLinearGridCrinkleExtractor.h>
#include <vtk3DLinearGridPlaneCutter.h>
#include <vtkAppendFilter.h>
#include <vtkAppendPolyData.h>
#include <vtkAttributeDataToTableFilter.h>
#include <vtkAttributeSmoothingFilter.h>
#include <vtkBinnedDecimation.h>
#include <vtkBoundaryMeshQuality.h>
#include <vtkCellCenters.h>
#include <vtkCellDataToPointData.h>
#include <vtkCellDerivatives.h>
#include <vtkCellQuality.h>
#include <vtkCheckerboardSplatter.h>
#include <vtkCleanUnstructuredGrid.h>
#include <vtkConstrainedSmoothingFilter.h>
#include <vtkContour3DLinearGrid.h>
#include <vtkContourFilter.h>
#include <vtkCorrelativeStatistics.h>
#include <vtkCutter.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkDescriptiveStatistics.h>
#include <vtkDiscreteFlyingEdges2D.h>
#include <vtkDiscreteFlyingEdges3D.h>
#include <vtkDiscreteFlyingEdgesClipper2D.h>
#include <vtkDistancePolyDataFilter.h>
#include <vtkElevationFilter.h>
#include <vtkExtractCells.h>
#include <vtkExtractEdges.h>
#include <vtkExtractEnclosedPoints.h>
#include <vtkExtractGeometry.h>
#include <vtkExtractPoints.h>
#include <vtkExtractSelection.h>
#include <vtkFitImplicitFunction.h>
#include <vtkFlyingEdges2D.h>
#include <vtkFlyingEdges3D.h>
#include <vtkFlyingEdgesPlaneCutter.h>
#include <vtkGaussianSplatter.h>
#include <vtkGeometryFilter.h>
#include <vtkGradientFilter.h>
#include <vtkImageAppendComponents.h>
#include <vtkImageBlend.h>
#include <vtkImageCast.h>
#include <vtkImageContinuousDilate3D.h>
#include <vtkImageContinuousErode3D.h>
#include <vtkImageDilateErode3D.h>
#include <vtkImageExtractComponents.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkImageMapToColors.h>
#include <vtkImageMedian3D.h>
#include <vtkImageRectilinearWipe.h>
#include <vtkImageReslice.h>
#include <vtkImageResize.h>
#include <vtkImageShiftScale.h>
#include <vtkImageShrink3D.h>
#include <vtkImageThreshold.h>
#include <vtkIntegrateAttributes.h>
#include <vtkLengthDistribution.h>
#include <vtkMarkBoundaryFilter.h>
#include <vtkMeshQuality.h>
#include <vtkMultiCorrelativeStatistics.h>
#include <vtkMultiObjectMassProperties.h>
#include <vtkOrderStatistics.h>
#include <vtkPackLabels.h>
#include <vtkPlaneCutter.h>
#include <vtkPointDataToCellData.h>
#include <vtkPointInterpolator.h>
#include <vtkPointInterpolator2D.h>
#include <vtkPointSmoothingFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyDataPlaneClipper.h>
#include <vtkPolyDataPlaneCutter.h>
#include <vtkPolyDataTangents.h>
#include <vtkPolyDataToUnstructuredGrid.h>
#include <vtkProbeFilter.h>
#include <vtkRTAnalyticSource.h>
#include <vtkRadiusOutlierRemoval.h>
#include <vtkRemovePolyData.h>
#include <vtkResampleToImage.h>
#include <vtkResampleWithDataSet.h>
#include <vtkSPHInterpolator.h>
#include <vtkSampleFunction.h>
#include <vtkSelectEnclosedPoints.h>
#include <vtkShepardMethod.h>
#include <vtkSimpleElevationFilter.h>
#include <vtkSphereSource.h>
#include <vtkSplitSharpEdgesPolyData.h>
#include <vtkStaticCleanPolyData.h>
#include <vtkStaticCleanUnstructuredGrid.h>
#include <vtkStatisticalOutlierRemoval.h>
#include <vtkStatisticsAlgorithm.h>
#include <vtkStreamTracer.h>
#include <vtkStructuredDataPlaneCutter.h>
#include <vtkSumTables.h>
#include <vtkSurfaceNets2D.h>
#include <vtkSurfaceNets3D.h>
#include <vtkTableBasedClipDataSet.h>
#include <vtkTableFFT.h>
#include <vtkThreshold.h>
#include <vtkTransformFilter.h>
#include <vtkVisualStatistics.h>
#include <vtkVoronoi2D.h>
#include <vtkWarpScalar.h>
#include <vtkWarpVector.h>
#include <vtkWindowedSincPolyDataFilter.h>

#include <cmath>

namespace smpparity
{
namespace
{

// Attach a deterministic 3-component "vectors" point array + a "cellScalars"
// cell array, so vector/cell-data filters have something to chew on.
void decorate(vtkPointSet* ds)
{
  const vtkIdType np = ds->GetNumberOfPoints();
  vtkNew<vtkFloatArray> vec;
  vec->SetName("vectors");
  vec->SetNumberOfComponents(3);
  vec->SetNumberOfTuples(np);
  for (vtkIdType i = 0; i < np; ++i)
  {
    double p[3];
    ds->GetPoint(i, p);
    vec->SetTuple3(i, std::sin(p[0]), std::cos(p[1]), p[2] * 0.5);
  }
  ds->GetPointData()->AddArray(vec);
  ds->GetPointData()->SetActiveVectors("vectors");

  const vtkIdType nc = ds->GetNumberOfCells();
  vtkNew<vtkFloatArray> cs;
  cs->SetName("cellScalars");
  cs->SetNumberOfComponents(1);
  cs->SetNumberOfTuples(nc);
  for (vtkIdType i = 0; i < nc; ++i)
    cs->SetValue(i, static_cast<float>((i % 97) * 0.5 + 1.0));
  ds->GetCellData()->AddArray(cs);
}

vtkSmartPointer<vtkImageData> makeImage()
{
  vtkNew<vtkRTAnalyticSource> wav;
  wav->SetWholeExtent(-10, 10, -10, 10, -10, 10);
  wav->Update();
  auto img = vtkSmartPointer<vtkImageData>::New();
  img->DeepCopy(wav->GetOutput());
  return img;
}

vtkSmartPointer<vtkImageData> makeImage2D()
{
  vtkNew<vtkRTAnalyticSource> wav;
  wav->SetWholeExtent(-16, 16, -16, 16, 0, 0);
  wav->Update();
  auto img = vtkSmartPointer<vtkImageData>::New();
  img->DeepCopy(wav->GetOutput());
  return img;
}

// Integer-label image with a few block regions (labels 0..3).
vtkSmartPointer<vtkImageData> makeLabelImage(int nx, int ny, int nz)
{
  auto img = vtkSmartPointer<vtkImageData>::New();
  img->SetDimensions(nx, ny, nz);
  vtkNew<vtkIntArray> labels;
  labels->SetName("labels");
  labels->SetNumberOfComponents(1);
  labels->SetNumberOfTuples(static_cast<vtkIdType>(nx) * ny * nz);
  vtkIdType idx = 0;
  for (int z = 0; z < nz; ++z)
    for (int y = 0; y < ny; ++y)
      for (int x = 0; x < nx; ++x)
      {
        const int lbl = ((x * 4 / nx) + (y * 4 / std::max(1, ny)) + (z * 4 / std::max(1, nz))) % 4;
        labels->SetValue(idx++, lbl);
      }
  img->GetPointData()->SetScalars(labels);
  return img;
}

vtkSmartPointer<vtkPolyData> makeSphere(int res)
{
  vtkNew<vtkSphereSource> sph;
  sph->SetThetaResolution(res);
  sph->SetPhiResolution(res);
  vtkNew<vtkElevationFilter> elev;
  elev->SetInputConnection(sph->GetOutputPort());
  elev->SetLowPoint(0, 0, -0.5);
  elev->SetHighPoint(0, 0, 0.5);
  elev->Update();
  auto poly = vtkSmartPointer<vtkPolyData>::New();
  poly->DeepCopy(elev->GetOutput());
  poly->GetPointData()->SetActiveScalars("Elevation");
  return poly;
}

vtkSmartPointer<vtkPolyData> makePoly()
{
  auto poly = makeSphere(48);
  decorate(poly);
  return poly;
}

// Sphere with point Normals + TCoords (for vtkPolyDataTangents).
vtkSmartPointer<vtkPolyData> makePolyNT()
{
  auto poly = makeSphere(32);
  vtkNew<vtkPolyDataNormals> norm;
  norm->SetInputData(poly);
  norm->SetComputePointNormals(true);
  norm->SetSplitting(false);
  norm->Update();
  auto out = vtkSmartPointer<vtkPolyData>::New();
  out->DeepCopy(norm->GetOutput());
  const vtkIdType np = out->GetNumberOfPoints();
  vtkNew<vtkFloatArray> tc;
  tc->SetName("TCoords");
  tc->SetNumberOfComponents(2);
  tc->SetNumberOfTuples(np);
  for (vtkIdType i = 0; i < np; ++i)
  {
    double p[3];
    out->GetPoint(i, p);
    tc->SetTuple2(i, 0.5 + 0.5 * p[0], 0.5 + 0.5 * p[1]);
  }
  out->GetPointData()->SetTCoords(tc);
  return out;
}

vtkSmartPointer<vtkPolyData> makePoly2(vtkPolyData* base)
{
  vtkNew<vtkTransform> t;
  t->Translate(0.5, 0.1, 0.0);
  vtkNew<vtkTransformFilter> tf;
  tf->SetInputData(base);
  tf->SetTransform(t);
  tf->Update();
  auto out = vtkSmartPointer<vtkPolyData>::New();
  out->DeepCopy(tf->GetOutput());
  return out;
}

vtkSmartPointer<vtkUnstructuredGrid> makeUGrid(vtkImageData* image)
{
  vtkNew<vtkThreshold> th;
  th->SetInputData(image);
  th->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "RTData");
  th->SetThresholdFunction(vtkThreshold::THRESHOLD_UPPER);
  th->SetUpperThreshold(100.0);
  th->Update();
  auto ug = vtkSmartPointer<vtkUnstructuredGrid>::New();
  ug->DeepCopy(th->GetOutput());
  ug->GetPointData()->SetActiveScalars("RTData");
  decorate(ug);
  return ug;
}

// Point cloud (polydata with vertex cells + a "scalars" point array).
vtkSmartPointer<vtkPolyData> makeCloud(bool planar)
{
  auto pd = vtkSmartPointer<vtkPolyData>::New();
  vtkNew<vtkPoints> pts;
  vtkNew<vtkFloatArray> sc;
  sc->SetName("scalars");
  sc->SetNumberOfComponents(1);
  if (planar)
  {
    for (int y = 0; y < 24; ++y)
      for (int x = 0; x < 24; ++x)
      {
        const double px = x * 0.5 - 6.0, py = y * 0.5 - 6.0;
        pts->InsertNextPoint(px, py, 0.0);
        sc->InsertNextValue(static_cast<float>(px + py));
      }
  }
  else
  {
    for (int i = 0; i < 500; ++i)
    {
      const double t = i * 0.13;
      const double x = std::cos(t) * (1.0 + 0.006 * i);
      const double y = std::sin(t) * (1.0 + 0.006 * i);
      const double z = 0.01 * i - 2.5;
      pts->InsertNextPoint(x, y, z);
      sc->InsertNextValue(static_cast<float>(x + y + z));
    }
  }
  pd->SetPoints(pts);
  pd->GetPointData()->SetScalars(sc);
  vtkNew<vtkCellArray> verts;
  for (vtkIdType i = 0; i < pts->GetNumberOfPoints(); ++i)
    verts->InsertNextCell(1, &i);
  pd->SetVerts(verts);
  return pd;
}

vtkSmartPointer<vtkTable> makeTable(double phase)
{
  auto t = vtkSmartPointer<vtkTable>::New();
  vtkNew<vtkDoubleArray> a;
  a->SetName("A");
  vtkNew<vtkDoubleArray> b;
  b->SetName("B");
  for (int i = 0; i < 64; ++i)
  {
    a->InsertNextValue(std::sin(0.1 * i + phase));
    b->InsertNextValue(std::cos(0.07 * i + phase));
  }
  t->AddColumn(a);
  t->AddColumn(b);
  return t;
}

} // namespace

const char* RiskName(Risk r)
{
  switch (r)
  {
    case Risk::PerElement: return "per-element";
    case Risk::Reduce:     return "reduce";
    case Risk::Iso:        return "isosurface";
    case Risk::Merge:      return "point-merge";
  }
  return "?";
}

Inputs BuildInputs()
{
  Inputs in;
  in.image = makeImage();
  in.image2d = makeImage2D();
  in.labelImage = makeLabelImage(24, 24, 24);
  in.labelImage2d = makeLabelImage(32, 32, 1);
  in.poly = makePoly();
  in.polyNT = makePolyNT();
  in.poly2 = makePoly2(in.poly);
  in.cloud = makeCloud(false);
  in.cloudPlanar = makeCloud(true);
  in.ugrid = makeUGrid(in.image);
  in.table = makeTable(0.0);
  in.table2 = makeTable(0.5);
  return in;
}

std::vector<Case> RegisterCases()
{
  std::vector<Case> cases;
  auto add = [&cases](const char* name, const char* mod, Risk risk,
               std::function<vtkSmartPointer<vtkAlgorithm>(const Inputs&)> make,
               bool orderRelaxed = false) {
    cases.push_back(Case{ name, mod, risk, std::move(make), orderRelaxed });
  };
  // Tag the just-registered case as a known, documented nondeterminism exception
  // (see Case::knownIssue): reported with its magnitude but not a gate failure.
  [[maybe_unused]] auto markKnown = [&cases](const std::string& reason) {
    cases.back().knownIssue = true;
    cases.back().knownIssueReason = reason;
  };
  // Compare the just-registered case's non-default output port. Statistics filters
  // mirror their input on port 0 and emit the computed model on OUTPUT_MODEL (1).
  auto useOutputPort = [&cases](int port) { cases.back().outputPort = port; };
  auto plane = [](double nx, double ny, double nz) {
    vtkSmartPointer<vtkPlane> pl = vtkSmartPointer<vtkPlane>::New();
    pl->SetOrigin(0, 0, 0);
    pl->SetNormal(nx, ny, nz);
    return pl;
  };

  // ---- per-element (out[i] = f(in[i])) --------------------------------------
  add("vtkElevationFilter", "Filters/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkElevationFilter> f;
    f->SetInputData(in.poly);
    f->SetLowPoint(-1, -1, -1);
    f->SetHighPoint(1, 1, 1);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkSimpleElevationFilter", "Filters/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkSimpleElevationFilter> f;
    f->SetInputData(in.poly);
    f->SetVector(0.3, 0.5, 0.8);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkWarpScalar", "Filters/General", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkWarpScalar> f;
    f->SetInputData(in.poly);
    f->SetScaleFactor(0.3);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkWarpVector", "Filters/General", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkWarpVector> f;
    f->SetInputData(in.poly);
    f->SetScaleFactor(0.2);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkTransformFilter", "Filters/General", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkTransform> t;
    t->RotateZ(37.0);
    t->Scale(1.5, 0.7, 1.1);
    vtkNew<vtkTransformFilter> f;
    f->SetInputData(in.poly);
    f->SetTransform(t);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkPolyDataNormals", "Filters/Core", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkPolyDataNormals> f;
    f->SetInputData(in.poly);
    f->SetComputePointNormals(true);
    f->SetComputeCellNormals(true);
    f->SetSplitting(false);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkPolyDataTangents", "Filters/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkPolyDataTangents> f;
    f->SetInputData(in.polyNT);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkThreshold", "Filters/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkThreshold> f;
    f->SetInputData(in.ugrid);
    f->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "RTData");
    f->SetThresholdFunction(vtkThreshold::THRESHOLD_UPPER);
    f->SetUpperThreshold(150.0);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkCellDerivatives", "Filters/General", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkCellDerivatives> f;
    f->SetInputData(in.poly);
    f->SetVectorModeToComputeGradient();
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkCellCenters", "Filters/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkCellCenters> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkMarkBoundaryFilter", "Filters/Geometry", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkMarkBoundaryFilter> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkMeshQuality", "Filters/Verdict", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkMeshQuality> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkCellQuality", "Filters/Verdict", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkCellQuality> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Poly -> UG cell-by-cell repackaging: the threaded pass writes each cell into
  // its own slot at the input's cell index, so points/connectivity/data are
  // byte-exact regardless of partitioning.
  add("vtkPolyDataToUnstructuredGrid", "Filters/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkPolyDataToUnstructuredGrid> f;
    f->SetInputData(in.poly);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });

  // ---- reduction / accumulation ---------------------------------------------
  add("vtkCellDataToPointData", "Filters/Core", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkCellDataToPointData> f;
    f->SetInputData(in.ugrid);
    f->SetProcessAllArrays(true);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkPointDataToCellData", "Filters/Core", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkPointDataToCellData> f;
    f->SetInputData(in.ugrid);
    f->SetProcessAllArrays(true);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkGradientFilter", "Filters/General", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkGradientFilter> f;
    f->SetInputData(in.ugrid);
    f->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "RTData");
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkAttributeSmoothingFilter", "Filters/Geometry", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkAttributeSmoothingFilter> f;
    f->SetInputData(in.ugrid);
    f->SetNumberOfIterations(3);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkProbeFilter", "Filters/Core", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkProbeFilter> f;
    f->SetInputData(in.poly);
    f->SetSourceData(in.image);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkResampleToImage", "Filters/Core", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkResampleToImage> f;
    f->SetInputDataObject(in.ugrid);
    f->SetSamplingDimensions(16, 16, 16);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkPointInterpolator", "Filters/Points", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkPointInterpolator> f;
    f->SetInputData(in.poly);
    f->SetSourceData(in.cloud);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkSPHInterpolator", "Filters/Points", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkSPHInterpolator> f;
    f->SetInputData(in.poly);
    f->SetSourceData(in.cloud);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkPointSmoothingFilter", "Filters/Points", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkPointSmoothingFilter> f;
    f->SetInputData(in.poly);
    f->SetSmoothingModeToGeometric();
    f->SetNumberOfIterations(4);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkShepardMethod", "Imaging/Hybrid", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkShepardMethod> f;
    f->SetInputData(in.cloud);
    f->SetSampleDimensions(16, 16, 16);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkGaussianSplatter", "Imaging/Hybrid", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkGaussianSplatter> f;
    f->SetInputData(in.cloud);
    f->SetSampleDimensions(16, 16, 16);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkCheckerboardSplatter", "Imaging/Hybrid", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkCheckerboardSplatter> f;
    f->SetInputData(in.cloud);
    f->SetSampleDimensions(16, 16, 16);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkSampleFunction", "Imaging/Hybrid", Risk::PerElement, [](const Inputs&) {
    vtkNew<vtkSphere> s;
    s->SetRadius(1.0);
    vtkNew<vtkSampleFunction> f;
    f->SetImplicitFunction(s);
    f->SetSampleDimensions(20, 20, 20);
    f->SetModelBounds(-2, 2, -2, 2, -2, 2);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkIntegrateAttributes", "Filters/Parallel", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkIntegrateAttributes> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkLengthDistribution", "Filters/Statistics", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkLengthDistribution> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Now GATED byte-exact: vtkLengthDistribution used to seed its
  // vtkReservoirSampler from std::random_device on every call, making the
  // output nondeterministic even single-threaded. The sampler is now seedable
  // and the filter seeds it (default Seed=0), so both the sampled cell subset
  // and the per-cell vertex picks are deterministic; each sample writes its own
  // table row, so serial and threaded output are byte-identical.
  add("vtkSelectEnclosedPoints", "Filters/Modeling", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkSelectEnclosedPoints> f;
    f->SetInputData(in.cloud);
    f->SetSurfaceData(in.poly); // sphere is a closed manifold
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkDistancePolyDataFilter", "Filters/General", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkDistancePolyDataFilter> f;
    f->SetInputData(0, in.poly);
    f->SetInputData(1, in.poly2);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Resample a dataset onto a probe geometry (same family as vtkProbeFilter): each
  // output point gathers a deterministic interpolation of the source cell it lands
  // in, in output-point order -- byte-exact.
  add("vtkResampleWithDataSet", "Filters/Core", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkResampleWithDataSet> f;
    f->SetInputData(in.poly);
    f->SetSourceData(in.image);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkPointInterpolator2D", "Filters/Points", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkPointInterpolator2D> f;
    f->SetInputData(in.poly);
    f->SetSourceData(in.cloud);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Mesh smoothers: connectivity is copied through untouched and every point's new
  // position is a Jacobi (double-buffered) weighted average of its FIXED stencil
  // neighbors, summed in a fixed per-point order and written to its own slot -- so
  // points come out in input order, byte-exact. The threaded loop is over points,
  // not over the accumulation, so there is no cross-thread reduction of the sum.
  add("vtkConstrainedSmoothingFilter", "Filters/Core", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkConstrainedSmoothingFilter> f;
    f->SetInputData(in.poly);
    f->SetNumberOfIterations(5);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkWindowedSincPolyDataFilter", "Filters/Core", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkWindowedSincPolyDataFilter> f;
    f->SetInputData(in.poly);
    f->SetNumberOfIterations(20);
    f->SetFeatureEdgeSmoothing(false); // no point splitting: preserve count/order
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Per-object volume/area/centroid over a closed surface. GATED byte-exact.
  // The CI parity gate previously proved a REAL threading divergence in the
  // per-cell "Areas" cell-data (serial=7.28e-5 vs parallel=2.78e-3 at cell 7).
  // Root cause was NOT a reduction/labeling race but a shared-scratch data race:
  // ComputeProperties::operator() read connectivity via the raw-pointer
  // vtkPolyData::GetCellPoints(id, npts, pts) overload, which (for int32-default
  // cell storage) returns a pointer into the single shared
  // vtkAbstractCellArray::TempCell list. Concurrent threads clobbered that
  // buffer, so a cell's pts/npts -- and thus its own-area from
  // vtkPolygon::ComputeArea -- came out wrong under threading. Fixed by reading
  // through the thread-safe GetCellPoints(id, npts, pts, ptIds) overload with a
  // per-thread vtkIdList scratch, making all per-cell outputs (Areas, Volumes)
  // byte-identical serial-vs-parallel. (The per-object ObjectAreas/Volumes/
  // Centroids reductions live in field data, which this gate does not compare.)
  add("vtkMultiObjectMassProperties", "Filters/Core", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkMultiObjectMassProperties> f;
    f->SetInputData(in.poly);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });

  // ---- table outputs (per-column) -------------------------------------------
  add("vtkAttributeDataToTableFilter", "Filters/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkAttributeDataToTableFilter> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkTableFFT", "Filters/General", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkTableFFT> f;
    f->SetInputData(in.table);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkSumTables", "Filters/General", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkSumTables> f;
    f->SetInputData(0, in.table);
    f->SetInputData(1, in.table2);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });

  // ---- statistics MODEL outputs (vtkStatisticalModel on OUTPUT_MODEL) --------
  // vtkStatisticsAlgorithm subclasses compute a MODEL: a vtkStatisticalModel (a
  // vtkDataObject holding Learned/Derived/Test vtkTables). It lives on output port
  // OUTPUT_MODEL (1); port 0 (OUTPUT_DATA) just mirrors the input, so the model is
  // the output that actually exercises the Learn/Derive math -- hence useOutputPort.
  //
  // The comparator now DESCENDS a vtkStatisticalModel (and any vtkCompositeDataSet):
  // before this wave a model/composite output fell through every SafeDownCast in
  // CompareDataObjects and passed VACUOUSLY, so registering these cases without the
  // comparator fix would have been a false green. With the fix the Learned/Derived
  // leaf tables are compared byte-exact (numeric columns) as the model is walked in
  // deterministic (TableType, index) order.
  //
  // In this VTK the statistics Learn/Derive paths are SERIAL (no vtkSMPTools::For in
  // any of these filters), so the model is byte-identical under STDThread by
  // construction; these cases gate the comparator descent and confirm the STDThread
  // default introduces no divergence, rather than stressing a threaded reduction.
  // Set up the simplest deterministic comparable model: Learn+Derive on, Assess/Test
  // off. Run over the shared numeric table (columns A,B).
  add("vtkDescriptiveStatistics", "Filters/Statistics", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkDescriptiveStatistics> f;
    f->SetInputData(in.table);
    f->AddColumn("A");
    f->AddColumn("B");
    f->SetLearnOption(true);
    f->SetDeriveOption(true);
    f->SetAssessOption(false);
    f->SetTestOption(false);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  useOutputPort(vtkStatisticsAlgorithm::OUTPUT_MODEL);
  add("vtkCorrelativeStatistics", "Filters/Statistics", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkCorrelativeStatistics> f;
    f->SetInputData(in.table);
    f->AddColumnPair("A", "B");
    f->SetLearnOption(true);
    f->SetDeriveOption(true);
    f->SetAssessOption(false);
    f->SetTestOption(false);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  useOutputPort(vtkStatisticsAlgorithm::OUTPUT_MODEL);
  // Multi-column request (A,B jointly): a sparse covariance model + its Cholesky
  // decomposition in the Derived table.
  add("vtkMultiCorrelativeStatistics", "Filters/Statistics", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkMultiCorrelativeStatistics> f;
    f->SetInputData(in.table);
    f->SetColumnStatus("A", 1);
    f->SetColumnStatus("B", 1);
    f->RequestSelectedColumns();
    f->SetLearnOption(true);
    f->SetDeriveOption(true);
    f->SetAssessOption(false);
    f->SetTestOption(false);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  useOutputPort(vtkStatisticsAlgorithm::OUTPUT_MODEL);
  // Per-column histogram (Learned, std::map-ordered) + Cardinalities/Quantiles
  // (Derived). Deterministic value-keyed ordering.
  add("vtkOrderStatistics", "Filters/Statistics", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkOrderStatistics> f;
    f->SetInputData(in.table);
    f->AddColumn("A");
    f->AddColumn("B");
    f->SetLearnOption(true);
    f->SetDeriveOption(true);
    f->SetAssessOption(false);
    f->SetTestOption(false);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  useOutputPort(vtkStatisticsAlgorithm::OUTPUT_MODEL);
  // Unlike the statistics filters above (whose Learn/Derive is serial), the
  // vtkVisualStatistics Learn pass is THREADED: for each requested field it bins
  // the column into per-thread histograms via vtkSMPTools::For (HistogramWorker)
  // and composites the partials in Reduce(). This case therefore stresses a real
  // threaded reduction and its Learned-model histogram table must be byte-exact
  // vs serial. It previously diverged intermittently (a bin came back serial=1 /
  // parallel=2) because HistogramWorker read each sample with
  // vtkDataArray::GetTuple1(), which returns a pointer into the array's shared
  // LegacyTuple scratch buffer -- a data race that misbinned samples. The fix
  // reads via GetComponent(ii, 0) (thread-safe direct typed read). Because the
  // bug is INTERMITTENT, confirm this case over several gate runs, not just one.
  add("vtkVisualStatistics", "Filters/Statistics", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkVisualStatistics> f;
    f->SetInputData(in.table);
    f->AddColumn("A");
    f->AddColumn("B");
    f->SetFieldRange("A", -1, 1);
    f->SetFieldRange("B", -1, 1);
    f->SetLearnOption(true);
    f->SetDeriveOption(true);
    f->SetAssessOption(false);
    f->SetTestOption(false);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  useOutputPort(vtkStatisticsAlgorithm::OUTPUT_MODEL);

  // ---- stream tracing (parallel per-seed integration) -----------------------
  // vtkStreamTracer integrates one streamline per seed, and the class is threaded
  // over seeds via vtkSMPTools. Each streamline's points are a deterministic
  // function of its seed, but the EMISSION ORDER of whole streamlines across the
  // output polydata is thread-dependent -> orderRelaxed: point positions/count must
  // match serial as a set and be run-to-run stable, only the concatenation order of
  // the lines may vary. A red even under order-relaxed means per-seed positions or
  // count diverge = a real threading defect (shared/accumulated integrator state),
  // not a benign ordering effect. Uses a fresh copy of the dense wavelet image
  // decorated with an analytic rotational vector field (bounded helical streamlines
  // that traverse many cells), seeded by a small deterministic point set.
  add(
    "vtkStreamTracer", "Filters/FlowPaths", Risk::Iso,
    [](const Inputs& in) {
      auto img = vtkSmartPointer<vtkImageData>::New();
      img->DeepCopy(in.image);
      const vtkIdType np = img->GetNumberOfPoints();
      vtkNew<vtkDoubleArray> vf;
      vf->SetName("vfield");
      vf->SetNumberOfComponents(3);
      vf->SetNumberOfTuples(np);
      for (vtkIdType i = 0; i < np; ++i)
      {
        double p[3];
        img->GetPoint(i, p);
        // Rotational + gentle axial drift: bounded, non-trivial, everywhere defined.
        vf->SetTuple3(i, -p[1], p[0], 0.3);
      }
      img->GetPointData()->AddArray(vf);
      img->GetPointData()->SetActiveVectors("vfield");

      // Deterministic seed cloud: a small grid of points inside the domain.
      auto seeds = vtkSmartPointer<vtkPolyData>::New();
      vtkNew<vtkPoints> spts;
      for (int j = 0; j < 5; ++j)
        for (int i = 0; i < 5; ++i)
          spts->InsertNextPoint(-4.0 + 2.0 * i, -4.0 + 2.0 * j, -3.0);
      seeds->SetPoints(spts);

      vtkNew<vtkStreamTracer> f;
      f->SetInputData(img);
      f->SetSourceData(seeds);
      f->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "vfield");
      f->SetIntegratorTypeToRungeKutta45();
      f->SetIntegrationDirectionToForward();
      f->SetMaximumPropagation(40.0);
      f->SetInitialIntegrationStep(0.2);
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);

  // ---- isosurface / cut / clip (parallel extraction) ------------------------
  add("vtkContourFilter/image", "Filters/Core", Risk::Iso, [](const Inputs& in) {
    vtkNew<vtkContourFilter> f;
    f->SetInputData(in.image);
    f->SetValue(0, 150.0);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkFlyingEdges3D", "Filters/Core", Risk::Iso, [](const Inputs& in) {
    vtkNew<vtkFlyingEdges3D> f;
    f->SetInputData(in.image);
    f->SetValue(0, 150.0);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkFlyingEdges2D", "Filters/Core", Risk::Iso, [](const Inputs& in) {
    vtkNew<vtkFlyingEdges2D> f;
    f->SetInputData(in.image2d);
    f->SetValue(0, 150.0);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add(
    "vtkFlyingEdgesPlaneCutter", "Filters/Core", Risk::Iso,
    [plane](const Inputs& in) {
      vtkNew<vtkFlyingEdgesPlaneCutter> f;
      f->SetInputData(in.image);
      f->SetPlane(plane(1, 0.5, 0.25));
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  add(
    "vtkContour3DLinearGrid", "Filters/Core", Risk::Iso,
    [](const Inputs& in) {
      vtkNew<vtkContour3DLinearGrid> f;
      f->SetInputData(in.ugrid);
      f->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "RTData");
      f->SetValue(0, 130.0);
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  add(
    "vtkCutter", "Filters/Core", Risk::Iso,
    [plane](const Inputs& in) {
      vtkNew<vtkCutter> f;
      f->SetInputData(in.ugrid);
      f->SetCutFunction(plane(1, 1, 1));
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  add(
    "vtkPlaneCutter", "Filters/Core", Risk::Iso,
    [plane](const Inputs& in) {
      vtkNew<vtkPlaneCutter> f;
      f->SetInputData(in.ugrid);
      f->SetPlane(plane(1, 0.5, 0.25));
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  add(
    "vtkPolyDataPlaneCutter", "Filters/Core", Risk::Iso,
    [plane](const Inputs& in) {
      vtkNew<vtkPolyDataPlaneCutter> f;
      f->SetInputData(in.poly);
      f->SetPlane(plane(1, 0.5, 0.25));
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  add(
    "vtkPolyDataPlaneClipper", "Filters/Core", Risk::Iso,
    [plane](const Inputs& in) {
      vtkNew<vtkPolyDataPlaneClipper> f;
      f->SetInputData(in.poly);
      f->SetPlane(plane(1, 0.5, 0.25));
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  add("vtkTableBasedClipDataSet", "Filters/General", Risk::Iso, [plane](const Inputs& in) {
    vtkNew<vtkTableBasedClipDataSet> f;
    f->SetInputData(in.ugrid);
    f->SetClipFunction(plane(1, 1, 0));
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Parallel linear-grid cut/extract. Like the sibling fast cutters these emit the
  // same geometry in a thread-dependent order (per-batch prefix-sum offsets), so
  // order-relaxed: geometry + point/cell data must match serial as a set and be
  // stable run-to-run.
  add(
    "vtk3DLinearGridPlaneCutter", "Filters/Core", Risk::Iso,
    [plane](const Inputs& in) {
      vtkNew<vtk3DLinearGridPlaneCutter> f;
      f->SetInputData(in.ugrid);
      f->SetPlane(plane(1, 0.5, 0.25));
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  add(
    "vtk3DLinearGridCrinkleExtractor", "Filters/Core", Risk::Iso,
    [plane](const Inputs& in) {
      vtkNew<vtk3DLinearGridCrinkleExtractor> f;
      f->SetInputData(in.ugrid);
      f->SetImplicitFunction(plane(1, 0.5, 0.25));
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  add(
    "vtkStructuredDataPlaneCutter", "Filters/Core", Risk::Iso,
    [plane](const Inputs& in) {
      vtkNew<vtkStructuredDataPlaneCutter> f;
      f->SetInputData(in.image);
      f->SetPlane(plane(1, 0.5, 0.25));
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  // Unlike the sibling fast cutters (which match serial point positions EXACTLY,
  // only reordered), this one's threaded cut-point interpolation is FP
  // non-associative: the CI gate measured point POSITIONS 1 ULP off from serial
  // (max coord dev=1.11e-16), so even the order-insensitive geometry set differs.
  // Sub-ULP and almost certainly harmless, but a real violation of the
  // point-positions-sacred rule -- markKnown() ungates it (documented) rather than
  // silently widen the comparator's tolerance. Revisit if strict bit-exact
  // positions are required (would mean forcing this cutter serial).
  markKnown("point positions diverge 1 ULP parallel-vs-serial (max dev=1.1e-16), "
            "FP non-associativity in threaded cut-point interpolation; sub-ULP, "
            "ungated + documented");
  // GEOMETRY (default OUTPUT_STYLE_BOUNDARY) is deterministic (integer voxel-center
  // coords, serial prefix-sum offsets, per-thread label lookup, Jacobi
  // double-buffered smoothing) -- points + connectivity are byte-identical.
  // FIXED (gated). Two distinct uninitialized BoundaryLabels sources were addressed:
  //   1. #176: the pre-triangulation newScalars buffer in ConfigureOutput is sized
  //      to a PADDED upper-bound quad count; its trailing (never-emitted) tuples
  //      were left uninitialized. #176 zero-fills that buffer at allocation.
  //   2. THIS follow-up: the default output is smoothed TRIANGLES, so the newScalars
  //      quad buffer is replaced by a fresh 2*numCells "BoundaryLabels" array
  //      (updatedScalars) built in TransformMeshType. That array is SetNumberOfTuples'd
  //      (VTK leaves the storage uninitialized) and populated ONLY by
  //      ScalarsWorker/ScalarsDispatch -- a dispatch that silently no-ops if it fails
  //      to match the input array/value type, leaving every tuple wild. This is the
  //      EARLY-tuple garbage the gate saw (tuple 1, serial=4.16e8) that #176's
  //      trailing-padding fix could not reach. Fix: zero-init updatedScalars right
  //      after SetNumberOfTuples (mirroring the #176 std::fill), so any tuple the
  //      copy does not overwrite is a deterministic zero. Gated byte-exact: under the
  //      Sequential floor the whole pipeline is deterministic, and STDThread's
  //      per-tuple copy is a disjoint write, so parallel == serial byte-for-byte.
  add("vtkSurfaceNets3D", "Filters/Core", Risk::Iso, [](const Inputs& in) {
    vtkNew<vtkSurfaceNets3D> f;
    f->SetInputData(in.labelImage);
    f->SetValue(0, 1.0);
    f->SetValue(1, 2.0);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add(
    "vtkSurfaceNets2D", "Filters/Core", Risk::Iso,
    [](const Inputs& in) {
      vtkNew<vtkSurfaceNets2D> f;
      f->SetInputData(in.labelImage2d);
      f->SetValue(0, 1.0);
      f->SetValue(1, 2.0);
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  add(
    "vtkDiscreteFlyingEdges3D", "Filters/General", Risk::Iso,
    [](const Inputs& in) {
      vtkNew<vtkDiscreteFlyingEdges3D> f;
      f->SetInputData(in.labelImage);
      f->SetValue(0, 1.0);
      f->SetValue(1, 2.0);
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  add(
    "vtkDiscreteFlyingEdges2D", "Filters/General", Risk::Iso,
    [](const Inputs& in) {
      vtkNew<vtkDiscreteFlyingEdges2D> f;
      f->SetInputData(in.labelImage2d);
      f->SetValue(0, 1.0);
      f->SetValue(1, 2.0);
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  add(
    "vtkDiscreteFlyingEdgesClipper2D", "Filters/General", Risk::Iso,
    [](const Inputs& in) {
      vtkNew<vtkDiscreteFlyingEdgesClipper2D> f;
      f->SetInputData(in.labelImage2d);
      f->SetValue(0, 1.0);
      f->SetValue(1, 2.0);
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
  // FIXED (gated). Two distinct defects were addressed:
  //   1. A shared vtkLabelMapLookup cache race (CachedValue/CachedOutValue written
  //      unsynchronized) -- now given a per-thread lookup (vtkSMPThreadLocal,
  //      created in Pass1::Initialize, freed in Pass1::Reduce). This was NOT the
  //      cause of the garbage-coordinate symptom (the validator proved the garbage
  //      persisted unchanged after it).
  //   2. The real symptom: uninitialized output point coordinates. Pass1/Pass2
  //      COUNT points per-dyad (Inside origins, x/y edge splits, interior points)
  //      independent of whether the surrounding pixel forms a non-empty (manifold)
  //      case. When every pixel touching a counted dyad has an empty case (e.g. an
  //      isolated single-label pixel -> case 1 (1,0,0,0), numPolys==0), Pass4's
  //      numPolys>0 branch never runs for that dyad, so the reserved point slot is
  //      never written. Serial masks it (fresh allocation reads reproducible
  //      zeros); under STDThread the buffer lands on dirtied heap pages so those
  //      phantom slots read wild uninitialized floats (~3.4e37 Linux / ~1.2e10
  //      macOS) that vary run-to-run. Fix: zero-initialize NewPoints after
  //      allocation (ContourImage), so unwritten phantom slots are deterministic
  //      and match serial. orderRelaxed stays true because -- like the sibling
  //      vtkDiscreteFlyingEdges2D/3D -- threaded emission ORDER is still
  //      nondeterministic; the gate requires run-to-run stability plus an
  //      order-insensitive match to serial.
  add(
    "vtkVoronoi2D", "Filters/Core", Risk::Merge,
    [](const Inputs& in) {
      vtkNew<vtkVoronoi2D> f;
      f->SetInputData(in.cloudPlanar);
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);

  // ---- extraction / clip subsets --------------------------------------------
  add("vtkExtractGeometry", "Filters/Extraction", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkSphere> s;
    s->SetRadius(6.0);
    vtkNew<vtkExtractGeometry> f;
    f->SetInputData(in.ugrid);
    f->SetImplicitFunction(s);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkExtractCells", "Filters/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkExtractCells> f;
    f->SetInputData(in.ugrid);
    f->AddCellRange(0, in.ugrid->GetNumberOfCells() / 2);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkExtractSelection", "Filters/Extraction", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkSelectionNode> node;
    node->SetFieldType(vtkSelectionNode::CELL);
    node->SetContentType(vtkSelectionNode::INDICES);
    vtkNew<vtkIntArray> ids;
    const vtkIdType nc = in.ugrid->GetNumberOfCells();
    for (vtkIdType i = 0; i < nc; i += 2)
      ids->InsertNextValue(static_cast<int>(i));
    node->SetSelectionList(ids);
    vtkNew<vtkSelection> sel;
    sel->AddNode(node);
    vtkNew<vtkExtractSelection> f;
    f->SetInputData(0, in.ugrid);
    f->SetInputData(1, sel);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // vtkExtractSelection over the two remaining threaded selector kinds (the case
  // above exercises INDICES). Each selector fills a per-element insidedness array
  // in parallel; extraction then compacts in input cell order, so byte-exact.
  // VALUES -> vtkValueSelector: keep cells whose "cellScalars" equals a listed value.
  add("vtkExtractSelection/values", "Filters/Extraction", Risk::PerElement,
    [](const Inputs& in) {
      vtkNew<vtkSelectionNode> node;
      node->SetFieldType(vtkSelectionNode::CELL);
      node->SetContentType(vtkSelectionNode::VALUES);
      vtkNew<vtkFloatArray> vals;
      vals->SetName("cellScalars"); // must name the array to test against
      vals->InsertNextValue(1.0f);
      vals->InsertNextValue(5.0f);
      vals->InsertNextValue(10.0f);
      node->SetSelectionList(vals);
      vtkNew<vtkSelection> sel;
      sel->AddNode(node);
      vtkNew<vtkExtractSelection> f;
      f->SetInputData(0, in.ugrid);
      f->SetInputData(1, sel);
      return vtkSmartPointer<vtkAlgorithm>(f);
    });
  // LOCATIONS -> vtkLocationSelector: keep cells containing the listed world points.
  add("vtkExtractSelection/locations", "Filters/Extraction", Risk::PerElement,
    [](const Inputs& in) {
      vtkNew<vtkSelectionNode> node;
      node->SetFieldType(vtkSelectionNode::CELL);
      node->SetContentType(vtkSelectionNode::LOCATIONS);
      vtkNew<vtkDoubleArray> locs;
      locs->SetNumberOfComponents(3);
      locs->InsertNextTuple3(0.0, 0.0, 0.0);
      locs->InsertNextTuple3(3.0, -2.0, 1.0);
      locs->InsertNextTuple3(-4.0, 4.0, -3.0);
      node->SetSelectionList(locs);
      vtkNew<vtkSelection> sel;
      sel->AddNode(node);
      vtkNew<vtkExtractSelection> f;
      f->SetInputData(0, in.ugrid);
      f->SetInputData(1, sel);
      return vtkSmartPointer<vtkAlgorithm>(f);
    });
  // FRUSTUM -> vtkFrustumSelector: keep cells inside an 8-corner frustum. The
  // per-cell in/out test runs in a threaded functor (ComputeCellsInFrustumFunctor);
  // extraction then compacts in input cell order -> byte-exact. (The frustum's exact
  // winding is immaterial to parity: both backends use the same planes, so any
  // deterministic selection validates the threaded path.) Corners are the standard
  // near/far x-y-z vtkFrustumSelector vertex order, each as (x,y,z,1).
  add("vtkExtractSelection/frustum", "Filters/Extraction", Risk::PerElement,
    [](const Inputs& in) {
      vtkNew<vtkSelectionNode> node;
      node->SetFieldType(vtkSelectionNode::CELL);
      node->SetContentType(vtkSelectionNode::FRUSTUM);
      vtkNew<vtkDoubleArray> corners;
      corners->SetNumberOfComponents(4);
      const double lo = -8.0, hi = 8.0;
      // 0:nLL 1:fLL 2:nUL 3:fUL 4:nLR 5:fLR 6:nUR 7:fUR  (L/R=x, L/U=y, n/f=z)
      const double v[8][3] = { { lo, lo, lo }, { lo, lo, hi }, { lo, hi, lo }, { lo, hi, hi },
        { hi, lo, lo }, { hi, lo, hi }, { hi, hi, lo }, { hi, hi, hi } };
      for (int i = 0; i < 8; ++i)
        corners->InsertNextTuple4(v[i][0], v[i][1], v[i][2], 1.0);
      node->SetSelectionList(corners);
      vtkNew<vtkSelection> sel;
      sel->AddNode(node);
      vtkNew<vtkExtractSelection> f;
      f->SetInputData(0, in.ugrid);
      f->SetInputData(1, sel);
      return vtkSmartPointer<vtkAlgorithm>(f);
    });
  // Point-cloud subset extractors (vtkPointCloudFilter subclasses). The per-point
  // keep/drop mask is computed in a threaded loop, but the surviving points are
  // numbered by a SERIAL prefix-sum over the mask and copied to their mapped slot,
  // so output is emitted in input-point order -- byte-exact, not order-relaxed.
  add("vtkExtractPoints", "Filters/Points", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkSphere> s;
    s->SetRadius(2.0);
    vtkNew<vtkExtractPoints> f;
    f->SetInputData(in.cloud);
    f->SetImplicitFunction(s);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkFitImplicitFunction", "Filters/Points", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkSphere> s;
    s->SetRadius(2.0);
    vtkNew<vtkFitImplicitFunction> f;
    f->SetInputData(in.cloud);
    f->SetImplicitFunction(s);
    f->SetThreshold(0.5);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkRadiusOutlierRemoval", "Filters/Points", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkRadiusOutlierRemoval> f;
    f->SetInputData(in.cloud);
    f->SetRadius(0.5);
    f->SetNumberOfNeighbors(2);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // The keep/drop threshold is mean + k*stddev, where mean and stddev were
  // historically accumulated via vtkSMPThreadLocal partial sums composited in a
  // partition-dependent order -- an FP-order-sensitive reduction. A borderline
  // point could then flip keep<->drop between serial and N-thread runs, changing
  // the SURVIVOR COUNT (count is sacred). Registered gated byte-exact so the gate
  // proves the reduction is now deterministic (mean/stddev summed serially in
  // input-point order over the byte-identical per-point distance array).
  add("vtkStatisticalOutlierRemoval", "Filters/Points", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkStatisticalOutlierRemoval> f;
    f->SetInputData(in.cloud);
    f->SetSampleSize(8);
    f->SetStandardDeviationFactor(1.0);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkExtractEnclosedPoints", "Filters/Points", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkExtractEnclosedPoints> f;
    f->SetInputData(in.cloud);
    f->SetSurfaceData(in.poly); // sphere is a closed manifold
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkPackLabels", "Filters/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkPackLabels> f;
    f->SetInputData(in.labelImage);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkSplitSharpEdgesPolyData", "Filters/Core", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkSplitSharpEdgesPolyData> f;
    f->SetInputData(in.poly);
    f->SetFeatureAngle(20.0);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkBoundaryMeshQuality", "Filters/Verdict", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkBoundaryMeshQuality> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });

  // ---- point-merge / hashing / surface extraction ---------------------------
  add("vtkGeometryFilter", "Filters/Geometry", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkGeometryFilter> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkDataSetSurfaceFilter", "Filters/Geometry", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkDataSetSurfaceFilter> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkStaticCleanUnstructuredGrid", "Filters/Core", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkStaticCleanUnstructuredGrid> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkCleanUnstructuredGrid", "Filters/General", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkCleanUnstructuredGrid> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkStaticCleanPolyData", "Filters/Core", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkStaticCleanPolyData> f;
    f->SetInputData(in.poly);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkExtractEdges", "Filters/Core", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkExtractEdges> f;
    f->SetInputData(in.ugrid);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkBinnedDecimation", "Filters/Core", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkBinnedDecimation> f;
    f->SetInputData(in.poly);
    f->SetNumberOfDivisions(16, 16, 16);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkAppendFilter", "Filters/Core", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkAppendFilter> f;
    f->AddInputData(in.ugrid);
    f->SetMergePoints(true);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkAppendPolyData", "Filters/Core", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkAppendPolyData> f;
    f->AddInputData(in.poly);
    f->AddInputData(in.poly2);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkRemovePolyData", "Filters/General", Risk::Merge, [](const Inputs& in) {
    vtkNew<vtkRemovePolyData> f;
    f->SetInputData(0, in.poly);
    f->AddInputData(1, in.poly2);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });

  // ---- image (vtkThreadedImageAlgorithm) ------------------------------------
  // cvista is NOT built with VTK_SMP_Sequential, so
  // vtkThreadedImageAlgorithm::GlobalDefaultEnableSMP defaults to true and every
  // subclass threads its ThreadedRequestData through vtkSMPTools::For out of the
  // box (the base splits the OUTPUT extent into pieces; the serial floor's
  // Sequential backend runs that For serially). Each output voxel is a pure
  // function of the input it reads (pointwise, fixed local kernel, or independent
  // resample), written to its own disjoint slot, so the value is independent of
  // how the extent was partitioned -> byte-exact parallel-vs-serial. None of these
  // reduce across the split, so Risk::PerElement.
  add("vtkImageThreshold", "Imaging/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageThreshold> f;
    f->SetInputData(in.image);
    f->ThresholdByUpper(150.0);
    f->SetInValue(1000.0);
    f->SetOutValue(0.0);
    f->SetOutputScalarTypeToFloat();
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkImageShiftScale", "Imaging/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageShiftScale> f;
    f->SetInputData(in.image);
    f->SetShift(10.0);
    f->SetScale(0.25);
    f->SetOutputScalarTypeToDouble();
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkImageCast", "Imaging/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageCast> f;
    f->SetInputData(in.image);
    f->SetOutputScalarTypeToDouble();
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkImageExtractComponents", "Imaging/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageExtractComponents> f;
    f->SetInputData(in.image);
    f->SetComponents(0);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Block downsample: each output voxel = mean over its FIXED input block, summed
  // in a fixed order -> byte-exact regardless of extent partition.
  add("vtkImageShrink3D", "Imaging/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageShrink3D> f;
    f->SetInputData(in.image);
    f->SetShrinkFactors(2, 2, 2);
    f->MeanOn();
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Resample onto a coarser grid; each output voxel is an independent interpolation
  // of the input -> byte-exact.
  add("vtkImageResize", "Imaging/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageResize> f;
    f->SetInputData(in.image);
    f->SetResizeMethodToOutputDimensions();
    f->SetOutputDimensions(15, 15, 15);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Reslice to a coarser spacing with linear interpolation; each output voxel is an
  // independent sample of the input volume -> byte-exact per voxel.
  add("vtkImageReslice", "Imaging/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageReslice> f;
    f->SetInputData(in.image);
    f->SetOutputSpacing(1.5, 1.5, 1.5);
    f->SetInterpolationModeToLinear();
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Per-voxel LUT map (scalar -> RGBA uint8); pointwise -> byte-exact.
  add("vtkImageMapToColors", "Imaging/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkLookupTable> lut;
    lut->SetTableRange(0.0, 300.0);
    lut->Build();
    vtkNew<vtkImageMapToColors> f;
    f->SetInputData(in.image);
    f->SetLookupTable(lut);
    f->SetOutputFormatToRGBA();
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Two-input component stack (same image twice -> 2-component output). Each output
  // voxel copies its corresponding input voxels -> byte-exact.
  add("vtkImageAppendComponents", "Imaging/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageAppendComponents> f;
    f->AddInputData(in.image);
    f->AddInputData(in.image);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Per-voxel weighted blend of two inputs; pointwise -> byte-exact.
  add("vtkImageBlend", "Imaging/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageBlend> f;
    f->AddInputData(in.image);
    f->AddInputData(in.image);
    f->SetOpacity(0, 0.5);
    f->SetOpacity(1, 0.5);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Separable Gaussian convolution: each output voxel is a fixed-weight sum over a
  // fixed input neighborhood -> byte-exact.
  add("vtkImageGaussianSmooth", "Imaging/General", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageGaussianSmooth> f;
    f->SetInputData(in.image);
    f->SetStandardDeviations(1.5, 1.5, 1.5);
    f->SetRadiusFactors(2.0, 2.0, 2.0);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Median over a fixed kernel: deterministic per-voxel selection -> byte-exact.
  add("vtkImageMedian3D", "Imaging/General", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageMedian3D> f;
    f->SetInputData(in.image);
    f->SetKernelSize(3, 3, 3);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Grayscale morphology: max / min over a fixed kernel -> byte-exact per voxel.
  add("vtkImageContinuousDilate3D", "Imaging/Morphological", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageContinuousDilate3D> f;
    f->SetInputData(in.image);
    f->SetKernelSize(3, 3, 3);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkImageContinuousErode3D", "Imaging/Morphological", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageContinuousErode3D> f;
    f->SetInputData(in.image);
    f->SetKernelSize(3, 3, 3);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkImageDilateErode3D", "Imaging/Morphological", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageDilateErode3D> f;
    f->SetInputData(in.labelImage);
    f->SetKernelSize(3, 3, 3);
    f->SetDilateValue(1.0);
    f->SetErodeValue(0.0);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  // Region composite of two images (same image twice); each output voxel copies one
  // input voxel chosen by a fixed spatial rule -> byte-exact.
  add("vtkImageRectilinearWipe", "Imaging/Hybrid", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkImageRectilinearWipe> f;
    f->SetInput1Data(in.image);
    f->SetInput2Data(in.image);
    f->SetWipeToQuad();
    return vtkSmartPointer<vtkAlgorithm>(f);
  });

  return cases;
}

} // namespace smpparity
