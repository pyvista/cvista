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
#include <vtkIdList.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkStaticPointLocator.h>
#include <vtkTable.h>
#include <vtkUnstructuredGrid.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

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

  // Tables: compare the row-data columns byte-exact.
  if (auto* ta = vtkTable::SafeDownCast(a))
  {
    auto* tb = vtkTable::SafeDownCast(b);
    if (ta->GetNumberOfRows() != tb->GetNumberOfRows())
      return "table rows " + std::to_string(ta->GetNumberOfRows()) + " vs " +
        std::to_string(tb->GetNumberOfRows());
    return compareAttributes("rowdata", ta->GetRowData(), tb->GetRowData());
  }

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

namespace
{

// Append the raw bytes of a value to a key buffer.
template <typename T>
void pushBytes(std::string& key, T v)
{
  key.append(reinterpret_cast<const char*>(&v), sizeof(T));
}

// Per-point canonical key: coordinates (as double, exact-widened from float) plus
// every point-data component. Two points are "the same" iff their keys are equal.
std::string pointKey(vtkDataSet* ds, vtkIdType i)
{
  std::string key;
  double p[3];
  ds->GetPoint(i, p);
  pushBytes(key, p[0]);
  pushBytes(key, p[1]);
  pushBytes(key, p[2]);
  vtkPointData* pd = ds->GetPointData();
  for (int a = 0; a < pd->GetNumberOfArrays(); ++a)
  {
    vtkDataArray* arr = pd->GetArray(a);
    if (!arr)
      continue;
    for (int c = 0; c < arr->GetNumberOfComponents(); ++c)
      pushBytes(key, arr->GetComponent(i, c));
  }
  return key;
}

std::string cellDataKey(vtkDataSet* ds, vtkIdType i)
{
  std::string key;
  vtkCellData* cd = ds->GetCellData();
  for (int a = 0; a < cd->GetNumberOfArrays(); ++a)
  {
    vtkDataArray* arr = cd->GetArray(a);
    if (!arr)
      continue;
    for (int c = 0; c < arr->GetNumberOfComponents(); ++c)
      pushBytes(key, arr->GetComponent(i, c));
  }
  return key;
}

// Smallest cyclic rotation of a vertex ring, ORIENTATION-preserving (no reflect).
// Makes a triangle emitted as (A,B,C), (B,C,A) or (C,A,B) compare identically --
// same triangle, different start vertex from thread batching -- while a flipped
// winding (A,C,B) stays distinct. Vertices are identified by their point key
// (coords+point-data), so duplicate/coincident points are unambiguous.
std::string canonicalRing(const std::vector<std::string>& verts)
{
  const std::size_t n = verts.size();
  if (n == 0)
    return "";
  std::size_t best = 0;
  for (std::size_t s = 1; s < n; ++s)
  {
    for (std::size_t k = 0; k < n; ++k)
    {
      const std::string& vs = verts[(s + k) % n];
      const std::string& vb = verts[(best + k) % n];
      if (vs < vb)
      {
        best = s;
        break;
      }
      if (vb < vs)
        break;
    }
  }
  std::string out;
  for (std::size_t k = 0; k < n; ++k)
  {
    out += verts[(best + k) % n];
    out.push_back('|');
  }
  return out;
}

// The multiset of cell descriptors: cell type + orientation-canonical ring of
// vertex point-keys + cell-data key. Independent of point IDs and of the order
// cells/points are stored in.
std::vector<std::string> cellDescriptors(vtkDataSet* ds)
{
  const vtkIdType np = ds->GetNumberOfPoints();
  std::vector<std::string> pkey(np);
  for (vtkIdType i = 0; i < np; ++i)
    pkey[i] = pointKey(ds, i);

  const vtkIdType nc = ds->GetNumberOfCells();
  std::vector<std::string> descs(nc);
  vtkNew<vtkIdList> ids;
  for (vtkIdType i = 0; i < nc; ++i)
  {
    ds->GetCellPoints(i, ids);
    std::vector<std::string> verts(ids->GetNumberOfIds());
    for (vtkIdType j = 0; j < ids->GetNumberOfIds(); ++j)
      verts[j] = pkey[ids->GetId(j)];
    std::string d;
    pushBytes(d, ds->GetCellType(i));
    d += canonicalRing(verts);
    d += cellDataKey(ds, i);
    descs[i] = std::move(d);
  }
  std::sort(descs.begin(), descs.end());
  return descs;
}

std::vector<std::string> sortedPointKeys(vtkDataSet* ds)
{
  const vtkIdType np = ds->GetNumberOfPoints();
  std::vector<std::string> keys(np);
  for (vtkIdType i = 0; i < np; ++i)
    keys[i] = pointKey(ds, i);
  std::sort(keys.begin(), keys.end());
  return keys;
}

// Max over every point of `from` of the distance to its nearest point in `to`.
// Symmetrized by the caller. This quantifies a "same count, different points"
// divergence: ~1e-12 means the two point clouds are the SAME geometry to
// floating-point noise (benign nondeterministic reduction order); a macroscopic
// value means genuinely different geometry (a real bug). Only invoked on the
// (rare) failure path, so the O(n log n) locator build is not a hot cost.
double maxNearestDeviation(vtkDataSet* from, vtkDataSet* to)
{
  if (from->GetNumberOfPoints() == 0 || to->GetNumberOfPoints() == 0)
    return 0.0;
  vtkNew<vtkStaticPointLocator> loc;
  loc->SetDataSet(to);
  loc->BuildLocator();
  double maxd = 0.0;
  double p[3], q[3];
  for (vtkIdType i = 0, n = from->GetNumberOfPoints(); i < n; ++i)
  {
    from->GetPoint(i, p);
    const vtkIdType j = loc->FindClosestPoint(p);
    to->GetPoint(j, q);
    const double d = std::sqrt(vtkMath::Distance2BetweenPoints(p, q));
    if (d > maxd)
      maxd = d;
  }
  return maxd;
}

// Symmetric Hausdorff-style point deviation, formatted for a diagnostic message.
std::string pointDeviation(vtkDataSet* a, vtkDataSet* b)
{
  const double dev = std::max(maxNearestDeviation(a, b), maxNearestDeviation(b, a));
  std::ostringstream os;
  os << " [max coord dev=" << std::scientific << dev << "]";
  return os.str();
}

// Order-insensitive table comparison: each column's VALUES compared as a sorted
// multiset. If a filter collects rows in a thread-dependent order but the value
// set is identical, this passes. On mismatch it reports the largest gap between
// the two sorted value sequences so FP-noise (~1e-12) is distinguishable from a
// genuine value divergence.
std::string compareTableUnordered(vtkTable* ta, vtkTable* tb)
{
  const vtkIdType nra = ta->GetNumberOfRows();
  const vtkIdType nrb = tb->GetNumberOfRows();
  if (nra != nrb)
    return "table rows " + std::to_string(nra) + " vs " + std::to_string(nrb);
  vtkDataSetAttributes* ra = ta->GetRowData();
  vtkDataSetAttributes* rb = tb->GetRowData();
  if (ra->GetNumberOfArrays() != rb->GetNumberOfArrays())
    return "table column count differs";
  for (int a = 0; a < ra->GetNumberOfArrays(); ++a)
  {
    vtkDataArray* ca = ra->GetArray(a);
    if (!ca)
      continue;
    const char* nm = ca->GetName();
    vtkDataArray* cb = nm ? rb->GetArray(nm) : rb->GetArray(a);
    const std::string label = std::string("column[") + (nm ? nm : "#") + "]";
    if (!cb)
      return label + ": missing on parallel side";
    if (ca->GetNumberOfComponents() != cb->GetNumberOfComponents())
      return label + ": ncomp differs";
    const int nc = ca->GetNumberOfComponents();
    std::vector<double> va, vb;
    va.reserve(nra * nc);
    vb.reserve(nrb * nc);
    for (vtkIdType i = 0; i < nra; ++i)
      for (int c = 0; c < nc; ++c)
      {
        va.push_back(ca->GetComponent(i, c));
        vb.push_back(cb->GetComponent(i, c));
      }
    std::sort(va.begin(), va.end());
    std::sort(vb.begin(), vb.end());
    double maxGap = 0.0;
    for (std::size_t i = 0; i < va.size(); ++i)
      maxGap = std::max(maxGap, std::abs(va[i] - vb[i]));
    if (maxGap != 0.0)
    {
      std::ostringstream os;
      os << label << ": value multiset differs [max sorted gap=" << std::scientific << maxGap
         << "]";
      return os.str();
    }
  }
  return "";
}

} // namespace

std::string CompareGeometrySet(vtkDataObject* a, vtkDataObject* b)
{
  if (!a || !b)
    return "null output object";

  // Tables (statistics filters): compare column values as sorted multisets.
  if (auto* ta = vtkTable::SafeDownCast(a))
    return compareTableUnordered(ta, vtkTable::SafeDownCast(b));

  vtkDataSet* dsa = vtkDataSet::SafeDownCast(a);
  vtkDataSet* dsb = vtkDataSet::SafeDownCast(b);
  if (!dsa || !dsb)
    return CompareDataObjects(a, b); // non-datasets: fall back to byte-exact

  if (dsa->GetNumberOfPoints() != dsb->GetNumberOfPoints())
    return "point count " + std::to_string(dsa->GetNumberOfPoints()) + " vs " +
      std::to_string(dsb->GetNumberOfPoints());
  if (dsa->GetNumberOfCells() != dsb->GetNumberOfCells())
    return "cell count " + std::to_string(dsa->GetNumberOfCells()) + " vs " +
      std::to_string(dsb->GetNumberOfCells());

  // Points (with point data) must match as a multiset. On mismatch, report the
  // max coordinate deviation so FP-noise is distinguishable from real divergence.
  if (sortedPointKeys(dsa) != sortedPointKeys(dsb))
    return "point set differs (same count, different points/point-data)" +
      pointDeviation(dsa, dsb);

  // Cells (orientation-canonical, key-based) must match as a multiset. A cell
  // mismatch is usually downstream of a point-coordinate mismatch, so annotate
  // with the point deviation magnitude too.
  if (cellDescriptors(dsa) != cellDescriptors(dsb))
    return "cell set differs (same count, different cells/connectivity/cell-data)" +
      pointDeviation(dsa, dsb);

  return "";
}

} // namespace smpparity
