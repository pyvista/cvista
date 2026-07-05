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
#include <vtkContour3DLinearGrid.h>
#include <vtkContourFilter.h>
#include <vtkCutter.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkDiscreteFlyingEdges2D.h>
#include <vtkDiscreteFlyingEdges3D.h>
#include <vtkDiscreteFlyingEdgesClipper2D.h>
#include <vtkDistancePolyDataFilter.h>
#include <vtkElevationFilter.h>
#include <vtkExtractCells.h>
#include <vtkExtractEdges.h>
#include <vtkExtractGeometry.h>
#include <vtkExtractSelection.h>
#include <vtkFlyingEdges2D.h>
#include <vtkFlyingEdges3D.h>
#include <vtkFlyingEdgesPlaneCutter.h>
#include <vtkGaussianSplatter.h>
#include <vtkGeometryFilter.h>
#include <vtkGradientFilter.h>
#include <vtkIntegrateAttributes.h>
#include <vtkLengthDistribution.h>
#include <vtkMarkBoundaryFilter.h>
#include <vtkMeshQuality.h>
#include <vtkPackLabels.h>
#include <vtkPlaneCutter.h>
#include <vtkPointDataToCellData.h>
#include <vtkPointInterpolator.h>
#include <vtkPointSmoothingFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyDataPlaneClipper.h>
#include <vtkPolyDataPlaneCutter.h>
#include <vtkPolyDataTangents.h>
#include <vtkProbeFilter.h>
#include <vtkRTAnalyticSource.h>
#include <vtkRemovePolyData.h>
#include <vtkResampleToImage.h>
#include <vtkSPHInterpolator.h>
#include <vtkSampleFunction.h>
#include <vtkSelectEnclosedPoints.h>
#include <vtkShepardMethod.h>
#include <vtkSimpleElevationFilter.h>
#include <vtkSphereSource.h>
#include <vtkSplitSharpEdgesPolyData.h>
#include <vtkStaticCleanPolyData.h>
#include <vtkStaticCleanUnstructuredGrid.h>
#include <vtkSumTables.h>
#include <vtkSurfaceNets2D.h>
#include <vtkSurfaceNets3D.h>
#include <vtkTableBasedClipDataSet.h>
#include <vtkTableFFT.h>
#include <vtkThreshold.h>
#include <vtkTransformFilter.h>
#include <vtkVoronoi2D.h>
#include <vtkWarpScalar.h>
#include <vtkWarpVector.h>

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
  add(
    "vtkSurfaceNets3D", "Filters/Core", Risk::Iso,
    [](const Inputs& in) {
      vtkNew<vtkSurfaceNets3D> f;
      f->SetInputData(in.labelImage);
      f->SetValue(0, 1.0);
      f->SetValue(1, 2.0);
      return vtkSmartPointer<vtkAlgorithm>(f);
    },
    /*orderRelaxed=*/true);
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

  return cases;
}

} // namespace smpparity
