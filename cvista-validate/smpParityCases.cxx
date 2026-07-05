// SPDX-License-Identifier: BSD-3-Clause
#include "smpParityCases.h"

#include <vtkAlgorithm.h>
#include <vtkCellData.h>
#include <vtkDataObject.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPlane.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkTransform.h>
#include <vtkUnstructuredGrid.h>

// Sources / filters under test.
#include <vtkAppendPolyData.h>
#include <vtkCellDataToPointData.h>
#include <vtkContour3DLinearGrid.h>
#include <vtkContourFilter.h>
#include <vtkCutter.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkElevationFilter.h>
#include <vtkExtractEdges.h>
#include <vtkFlyingEdges3D.h>
#include <vtkGeometryFilter.h>
#include <vtkGradientFilter.h>
#include <vtkPlaneCutter.h>
#include <vtkPointDataToCellData.h>
#include <vtkPolyDataNormals.h>
#include <vtkProbeFilter.h>
#include <vtkRTAnalyticSource.h>
#include <vtkSphereSource.h>
#include <vtkStaticCleanPolyData.h>
#include <vtkStaticCleanUnstructuredGrid.h>
#include <vtkTableBasedClipDataSet.h>
#include <vtkThreshold.h>
#include <vtkTransformFilter.h>
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
    // A smooth, position-dependent field (no RNG -> reproducible).
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
  {
    cs->SetValue(i, static_cast<float>((i % 97) * 0.5 + 1.0));
  }
  ds->GetCellData()->AddArray(cs);
}

vtkSmartPointer<vtkImageData> makeImage()
{
  vtkNew<vtkRTAnalyticSource> wav;
  wav->SetWholeExtent(-10, 10, -10, 10, -10, 10); // 21^3, scalar "RTData" (float)
  wav->Update();
  auto img = vtkSmartPointer<vtkImageData>::New();
  img->DeepCopy(wav->GetOutput());
  return img;
}

vtkSmartPointer<vtkPolyData> makePoly()
{
  vtkNew<vtkSphereSource> sph;
  sph->SetThetaResolution(48);
  sph->SetPhiResolution(48);
  vtkNew<vtkElevationFilter> elev;
  elev->SetInputConnection(sph->GetOutputPort());
  elev->SetLowPoint(0, 0, -0.5);
  elev->SetHighPoint(0, 0, 0.5);
  elev->Update();
  auto poly = vtkSmartPointer<vtkPolyData>::New();
  poly->DeepCopy(elev->GetOutput());
  poly->GetPointData()->SetActiveScalars("Elevation");
  decorate(poly);
  return poly;
}

vtkSmartPointer<vtkUnstructuredGrid> makeUGrid(vtkImageData* image)
{
  vtkNew<vtkThreshold> th;
  th->SetInputData(image);
  th->SetInputArrayToProcess(
    0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "RTData");
  th->SetThresholdFunction(vtkThreshold::THRESHOLD_UPPER);
  th->SetUpperThreshold(100.0);
  th->Update();
  auto ug = vtkSmartPointer<vtkUnstructuredGrid>::New();
  ug->DeepCopy(th->GetOutput());
  ug->GetPointData()->SetActiveScalars("RTData");
  decorate(ug);
  return ug;
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
  in.poly = makePoly();
  in.ugrid = makeUGrid(in.image);
  return in;
}

std::vector<Case> RegisterCases()
{
  std::vector<Case> cases;
  auto add = [&cases](const char* name, const char* mod, Risk risk,
               std::function<vtkSmartPointer<vtkAlgorithm>(const Inputs&)> make) {
    cases.push_back(Case{ name, mod, risk, std::move(make) });
  };

  // ---- per-element (the audited opt-in / RunSafeFilterParallel set) ----------
  add("vtkElevationFilter", "Filters/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkElevationFilter> f;
    f->SetInputData(in.poly);
    f->SetLowPoint(-1, -1, -1);
    f->SetHighPoint(1, 1, 1);
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
  add("vtkThreshold", "Filters/Core", Risk::PerElement, [](const Inputs& in) {
    vtkNew<vtkThreshold> f;
    f->SetInputData(in.ugrid);
    f->SetInputArrayToProcess(
      0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "RTData");
    f->SetThresholdFunction(vtkThreshold::THRESHOLD_UPPER);
    f->SetUpperThreshold(150.0);
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
    f->SetInputArrayToProcess(
      0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "RTData");
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkProbeFilter", "Filters/Core", Risk::Reduce, [](const Inputs& in) {
    vtkNew<vtkProbeFilter> f;
    f->SetInputData(in.poly);   // probe locations
    f->SetSourceData(in.image); // sampled field
    return vtkSmartPointer<vtkAlgorithm>(f);
  });

  // ---- isosurface / cut / clip ----------------------------------------------
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
  add("vtkContour3DLinearGrid", "Filters/Core", Risk::Iso, [](const Inputs& in) {
    vtkNew<vtkContour3DLinearGrid> f;
    f->SetInputData(in.ugrid);
    f->SetInputArrayToProcess(
      0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "RTData");
    f->SetValue(0, 130.0);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkCutter", "Filters/Core", Risk::Iso, [](const Inputs& in) {
    vtkNew<vtkPlane> pl;
    pl->SetOrigin(0, 0, 0);
    pl->SetNormal(1, 1, 1);
    vtkNew<vtkCutter> f;
    f->SetInputData(in.ugrid);
    f->SetCutFunction(pl);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkPlaneCutter", "Filters/Core", Risk::Iso, [](const Inputs& in) {
    vtkNew<vtkPlane> pl;
    pl->SetOrigin(0, 0, 0);
    pl->SetNormal(1, 0.5, 0.25);
    vtkNew<vtkPlaneCutter> f;
    f->SetInputData(in.ugrid);
    f->SetPlane(pl);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });
  add("vtkTableBasedClipDataSet", "Filters/General", Risk::Iso, [](const Inputs& in) {
    vtkNew<vtkPlane> pl;
    pl->SetOrigin(0, 0, 0);
    pl->SetNormal(1, 1, 0);
    vtkNew<vtkTableBasedClipDataSet> f;
    f->SetInputData(in.ugrid);
    f->SetClipFunction(pl);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });

  // ---- point-merge / hashing / surface extraction (highest risk) ------------
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
  add("vtkAppendPolyData", "Filters/Core", Risk::Merge, [](const Inputs& in) {
    // Second input: same sphere translated, so the append+merge has real work.
    vtkNew<vtkTransform> t;
    t->Translate(0.5, 0, 0);
    vtkNew<vtkTransformFilter> shift;
    shift->SetInputData(in.poly);
    shift->SetTransform(t);
    shift->Update();
    auto poly2 = vtkSmartPointer<vtkPolyData>::New();
    poly2->DeepCopy(shift->GetOutput());
    vtkNew<vtkAppendPolyData> f;
    f->AddInputData(in.poly);
    f->AddInputData(poly2);
    return vtkSmartPointer<vtkAlgorithm>(f);
  });

  return cases;
}

} // namespace smpparity
