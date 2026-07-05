// SPDX-License-Identifier: BSD-3-Clause
#include "smpParityCompare.h"

#include <vtkArrayDispatch.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDataArrayRange.h>
#include <vtkDataObject.h>
#include <vtkDataSet.h>
#include <vtkDataSetAttributes.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkUnstructuredGrid.h>

#include <cstring>
#include <sstream>
#include <string>
#include <type_traits>

namespace smpparity
{
namespace
{

// Bit-exact value comparison of two same-typed data arrays. We compare the raw
// bytes of every stored value (not operator==) so that NaN==NaN and +0.0!=-0.0
// behave the way an on-disk byte comparison would -- "same output" must mean
// identical bits, not merely numerically-equal. vtk::DataArrayValueRange routes
// through GetValue(), so implicit arrays (vtkConstantArray, the homogeneous
// cell-type representation, etc.) compare correctly too.
struct ValueEqWorker
{
  bool Equal = true;
  vtkIdType FirstBad = -1;

  // Dispatch2SameValueType guarantees the two arrays share a value type, but they
  // may be different array *classes* (AOS vs implicit), so take two type params.
  template <typename Array1T, typename Array2T>
  void operator()(Array1T* a, Array2T* b)
  {
    const auto ra = vtk::DataArrayValueRange(a);
    const auto rb = vtk::DataArrayValueRange(b);
    if (ra.size() != rb.size())
    {
      this->Equal = false;
      return;
    }
    using ValueT = vtk::GetAPIType<Array1T>;
    for (vtkIdType i = 0, n = ra.size(); i < n; ++i)
    {
      const ValueT va = ra[i];
      const ValueT vb = rb[i];
      if (std::memcmp(&va, &vb, sizeof(ValueT)) != 0)
      {
        this->Equal = false;
        this->FirstBad = i;
        return;
      }
    }
  }
};

// Universal exact fallback for array types outside the dispatch list (implicit
// arrays -- vtkConstantArray, the homogeneous cell-type representation -- whose
// GetVoidPointer(0) is null). Compares every component's bit pattern via the
// virtual GetComponent() double API. Exact for all float/double and for integer
// values within 2^53 (topology/type arrays in these fixtures never approach it).
std::string compareViaComponents(const std::string& what, vtkDataArray* a, vtkDataArray* b)
{
  const vtkIdType nt = a->GetNumberOfTuples();
  const int nc = a->GetNumberOfComponents();
  for (vtkIdType i = 0; i < nt; ++i)
  {
    for (int c = 0; c < nc; ++c)
    {
      const double va = a->GetComponent(i, c);
      const double vb = b->GetComponent(i, c);
      if (std::memcmp(&va, &vb, sizeof(double)) != 0)
        return what + ": values differ (tuple " + std::to_string(i) + ", comp " +
          std::to_string(c) + ")";
    }
  }
  return "";
}

std::string shape(vtkDataArray* a)
{
  std::ostringstream os;
  os << a->GetDataTypeAsString() << " " << a->GetNumberOfComponents() << "x"
     << a->GetNumberOfTuples();
  return os.str();
}

// "" if byte-identical, else a description of the first difference.
std::string compareArray(const std::string& what, vtkDataArray* a, vtkDataArray* b)
{
  if (!a && !b)
    return "";
  if (!a || !b)
    return what + ": one side null (" + (a ? "parallel" : "serial") + " missing)";
  if (a->GetDataType() != b->GetDataType())
    return what + ": dtype " + shape(a) + " vs " + shape(b);
  if (a->GetNumberOfComponents() != b->GetNumberOfComponents())
    return what + ": ncomp " + shape(a) + " vs " + shape(b);
  if (a->GetNumberOfTuples() != b->GetNumberOfTuples())
    return what + ": ntuples " + shape(a) + " vs " + shape(b);

  ValueEqWorker worker;
  using Dispatcher = vtkArrayDispatch::Dispatch2SameValueType;
  if (!Dispatcher::Execute(a, b, worker))
  {
    // Types outside the dispatch list (implicit arrays): exact component compare.
    return compareViaComponents(what, a, b);
  }
  if (!worker.Equal)
  {
    std::ostringstream os;
    os << what << ": values differ [" << shape(a) << "]";
    if (worker.FirstBad >= 0)
    {
      os << " first at value index " << worker.FirstBad;
      const vtkIdType comps = a->GetNumberOfComponents();
      if (comps > 0)
        os << " (tuple " << worker.FirstBad / comps << ", comp " << worker.FirstBad % comps
           << ")";
    }
    return os.str();
  }
  return "";
}

std::string compareAttributes(
  const std::string& tag, vtkDataSetAttributes* a, vtkDataSetAttributes* b)
{
  if (a->GetNumberOfArrays() != b->GetNumberOfArrays())
  {
    std::ostringstream os;
    os << tag << ": array count " << a->GetNumberOfArrays() << " vs " << b->GetNumberOfArrays();
    return os.str();
  }
  for (int i = 0; i < a->GetNumberOfArrays(); ++i)
  {
    vtkDataArray* aa = a->GetArray(i);
    const char* nm = aa ? aa->GetName() : nullptr;
    // Match by name when possible (array ORDER is allowed to be an output detail
    // only if names match); fall back to positional.
    vtkDataArray* bb = nm ? b->GetArray(nm) : b->GetArray(i);
    const std::string label = tag + "[" + (nm ? nm : "#") + std::to_string(i) + "]";
    const std::string d = compareArray(label, aa, bb);
    if (!d.empty())
      return d;
  }
  // Active-attribute assignments (scalars/vectors/normals) must also match.
  for (int attr = 0; attr < vtkDataSetAttributes::NUM_ATTRIBUTES; ++attr)
  {
    vtkDataArray* aa = a->GetAttribute(attr);
    vtkDataArray* bb = b->GetAttribute(attr);
    const char* an = aa ? aa->GetName() : "";
    const char* bn = bb ? bb->GetName() : "";
    if (std::strcmp(an ? an : "", bn ? bn : "") != 0)
      return tag + ": active attribute " + std::to_string(attr) + " differs";
  }
  return "";
}

std::string compareCellArray(const std::string& tag, vtkCellArray* a, vtkCellArray* b)
{
  if (!a && !b)
    return "";
  if (!a || !b)
    return tag + ": one side null";
  std::string d = compareArray(tag + ".offsets", a->GetOffsetsArray(), b->GetOffsetsArray());
  if (!d.empty())
    return d;
  return compareArray(tag + ".conn", a->GetConnectivityArray(), b->GetConnectivityArray());
}

} // namespace

std::string CompareDataObjects(vtkDataObject* a, vtkDataObject* b)
{
  if (!a || !b)
    return "null output object";
  if (std::strcmp(a->GetClassName(), b->GetClassName()) != 0)
    return std::string("class mismatch: ") + a->GetClassName() + " (serial) vs " +
      b->GetClassName() + " (parallel)";

  // Geometry: point coordinates for any point set.
  if (auto* pa = vtkPointSet::SafeDownCast(a))
  {
    auto* pb = vtkPointSet::SafeDownCast(b);
    vtkPoints* ga = pa->GetPoints();
    vtkPoints* gb = pb->GetPoints();
    const std::string d =
      compareArray("points", ga ? ga->GetData() : nullptr, gb ? gb->GetData() : nullptr);
    if (!d.empty())
      return d;
  }

  // Topology.
  if (auto* pa = vtkPolyData::SafeDownCast(a))
  {
    auto* pb = vtkPolyData::SafeDownCast(b);
    std::string d;
    if (!(d = compareCellArray("verts", pa->GetVerts(), pb->GetVerts())).empty())
      return d;
    if (!(d = compareCellArray("lines", pa->GetLines(), pb->GetLines())).empty())
      return d;
    if (!(d = compareCellArray("polys", pa->GetPolys(), pb->GetPolys())).empty())
      return d;
    if (!(d = compareCellArray("strips", pa->GetStrips(), pb->GetStrips())).empty())
      return d;
  }
  else if (auto* ua = vtkUnstructuredGrid::SafeDownCast(a))
  {
    auto* ub = vtkUnstructuredGrid::SafeDownCast(b);
    std::string d = compareCellArray("cells", ua->GetCells(), ub->GetCells());
    if (!d.empty())
      return d;
    d = compareArray("celltypes", ua->GetCellTypesArray(), ub->GetCellTypesArray());
    if (!d.empty())
      return d;
  }
  else if (auto* ia = vtkImageData::SafeDownCast(a))
  {
    auto* ib = vtkImageData::SafeDownCast(b);
    int da[3], db[3];
    ia->GetDimensions(da);
    ib->GetDimensions(db);
    if (std::memcmp(da, db, sizeof(da)) != 0)
      return "image dimensions differ";
    double oa[3], ob[3], sa[3], sb[3];
    ia->GetOrigin(oa);
    ib->GetOrigin(ob);
    ia->GetSpacing(sa);
    ib->GetSpacing(sb);
    if (std::memcmp(oa, ob, sizeof(oa)) != 0 || std::memcmp(sa, sb, sizeof(sa)) != 0)
      return "image origin/spacing differ";
  }

  // Attributes (point + cell data), for any dataset.
  vtkDataSet* dsa = vtkDataSet::SafeDownCast(a);
  vtkDataSet* dsb = vtkDataSet::SafeDownCast(b);
  if (dsa && dsb)
  {
    std::string d = compareAttributes("pointdata", dsa->GetPointData(), dsb->GetPointData());
    if (!d.empty())
      return d;
    d = compareAttributes("celldata", dsa->GetCellData(), dsb->GetCellData());
    if (!d.empty())
      return d;
  }
  return "";
}

} // namespace smpparity
