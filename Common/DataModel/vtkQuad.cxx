// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkQuad.h"

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkDoubleArray.h"
#include "vtkIncrementalPointLocator.h"
#include "vtkLine.h"
#include "vtkMarchingCellsClipCases.h"
#include "vtkMarchingCellsContourCases.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPlane.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkTriangle.h"

#include <algorithm> //std::copy
#include <array>

namespace
{
//------------------------------------------------------------------------------
[[maybe_unused]] constexpr const char* Topology = R"(
   Quad topology:

      3-----------2
      |           |
      |           |
      |           |
      0-----------1
)";

//------------------------------------------------------------------------------
double ParametricCoords[12] = {
  0.0, 0.0, 0.0, //
  1.0, 0.0, 0.0, //
  1.0, 1.0, 0.0, //
  0.0, 1.0, 0.0  //
};

//------------------------------------------------------------------------------
constexpr vtkIdType Edges[4][2] = {
  { 0, 1 },
  { 1, 2 },
  { 3, 2 },
  { 0, 3 },
};

constexpr double VTK_DIVERGED = 1.e6;
constexpr int VTK_MAX_ITERATIONS = 20;
constexpr double VTK_CONVERGED = 1.e-04;
}

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkQuad);

//------------------------------------------------------------------------------
struct IntersectionStruct
{
  bool Intersected = false;
  int SubId = -1;
  double X[3] = { 0.0, 0.0, 0.0 };
  double PCoords[3] = { 0.0, 0.0, 0.0 };
  double T = -1.0;

  operator bool() { return this->Intersected; }

  void CopyValues(double& t, double* x, double* pcoords, int& subId) const
  {
    t = this->T;
    subId = this->SubId;
    for (int i = 0; i < 3; ++i)
    {
      x[i] = this->X[i];
      pcoords[i] = this->PCoords[i];
    }
  }

  static IntersectionStruct CellIntersectWithLine(
    vtkCell* cell, const double* p1, const double* p2, double tol)
  {
    IntersectionStruct res;
    res.Intersected = cell->IntersectWithLine(p1, p2, tol, res.T, res.X, res.PCoords, res.SubId);
    return res;
  }
};

//------------------------------------------------------------------------------
// Construct the quad with four points.
vtkQuad::vtkQuad()
{
  this->Points->SetNumberOfPoints(4);
  this->PointIds->SetNumberOfIds(4);
  for (int i = 0; i < 4; i++)
  {
    this->Points->SetPoint(i, 0.0, 0.0, 0.0);
    this->PointIds->SetId(i, 0);
  }
  this->Line = vtkSmartPointer<vtkLine>::New();
  this->Triangle = vtkSmartPointer<vtkTriangle>::New();
}

//------------------------------------------------------------------------------
static void ComputeNormal(
  vtkQuad* self, const double* pt1, const double* pt2, const double* pt3, double n[3])
{
  vtkTriangle::ComputeNormal(pt1, pt2, pt3, n);

  // If first three points are co-linear, then use fourth point
  double pt4[3];
  if (n[0] == 0.0 && n[1] == 0.0 && n[2] == 0.0)
  {
    self->Points->GetPoint(3, pt4);
    vtkTriangle::ComputeNormal(pt2, pt3, pt4, n);
  }
}

//------------------------------------------------------------------------------
int vtkQuad::EvaluatePosition(const double x[3], double closestPoint[3], int& subId,
  double pcoords[3], double& dist2, double weights[])
{
  double n[3];
  double det;
  int idx = 0, indices[2];
  int converged;
  double params[2];
  double fcol[2], rcol[2], scol[2], cp[3];
  double derivs[8];

  subId = 0;
  pcoords[0] = pcoords[1] = params[0] = params[1] = 0.5;
  pcoords[2] = 0.0;

  // Efficient point access
  const auto pointsArray = vtkDoubleArray::FastDownCast(this->Points->GetData());
  if (!pointsArray)
  {
    vtkErrorMacro(<< "Points should be double type");
    return 0;
  }
  const double* pts = pointsArray->GetPointer(0);

  // Get normal for quadrilateral
  //
  const double* pt1 = pts;
  const double* pt2 = pts + 3;
  const double* pt3 = pts + 6;
  ComputeNormal(this, pt1, pt2, pt3, n);

  // Project point to plane
  //
  vtkPlane::ProjectPoint(x, pt1, n, cp);

  // Construct matrices.  Since we have over determined system, need to find
  // which 2 out of 3 equations to use to develop equations. (Any 2 should
  // work since we've projected point to plane.)
  double maxComponent = 0.0;
  for (int i = 0; i < 3; i++)
  {
    if (std::abs(n[i]) > maxComponent)
    {
      maxComponent = std::abs(n[i]);
      idx = i;
    }
  }
  for (int j = 0, i = 0; i < 3; i++)
  {
    if (i != idx)
    {
      indices[j++] = i;
    }
  }

  // Use Newton's method to solve for parametric coordinates
  //
  for (int iteration = converged = 0; !converged && iteration < VTK_MAX_ITERATIONS; ++iteration)
  {
    //  calculate element interpolation functions and derivatives
    //
    vtkQuad::InterpolationFunctions(pcoords, weights);
    vtkQuad::InterpolationDerivs(pcoords, derivs);

    //  calculate newton functions
    //
    fcol[0] = rcol[0] = scol[0] = 0.0;
    fcol[1] = rcol[1] = scol[1] = 0.0;

    for (int i = 0; i < 4; i++)
    {
      const double* pt = pts + 3 * i;
      fcol[0] += pt[indices[0]] * weights[i];
      rcol[0] += pt[indices[0]] * derivs[i];
      scol[0] += pt[indices[0]] * derivs[i + 4];
      fcol[1] += pt[indices[1]] * weights[i];
      rcol[1] += pt[indices[1]] * derivs[i];
      scol[1] += pt[indices[1]] * derivs[i + 4];
    }

    fcol[0] -= cp[indices[0]];
    fcol[1] -= cp[indices[1]];

    //  compute determinants and generate improvements
    //
    if ((det = vtkMath::Determinant2x2(rcol, scol)) == 0.0)
    {
      return -1;
    }

    pcoords[0] = params[0] - vtkMath::Determinant2x2(fcol, scol) / det;
    pcoords[1] = params[1] - vtkMath::Determinant2x2(rcol, fcol) / det;

    //  check for convergence
    if (std::abs(pcoords[0] - params[0]) < VTK_CONVERGED &&
      std::abs(pcoords[1] - params[1]) < VTK_CONVERGED)
    {
      converged = 1;
    }
    // Test for bad divergence (S.Hirschberg 11.12.2001)
    else if (std::abs(pcoords[0]) > VTK_DIVERGED || std::abs(pcoords[1]) > VTK_DIVERGED)
    {
      return -1;
    }
    //  if not converged, repeat
    else
    {
      params[0] = pcoords[0];
      params[1] = pcoords[1];
    }
  }

  //  if not converged, set the parametric coordinates to arbitrary values
  //  outside of element
  //
  if (!converged)
  {
    return -1;
  }

  vtkQuad::InterpolationFunctions(pcoords, weights);

  if (pcoords[0] >= -0.001 && pcoords[0] <= 1.001 && pcoords[1] >= -0.001 && pcoords[1] <= 1.001)
  {
    if (closestPoint)
    {
      dist2 = vtkMath::Distance2BetweenPoints(cp, x); // projection distance
      closestPoint[0] = cp[0];
      closestPoint[1] = cp[1];
      closestPoint[2] = cp[2];
    }
    return 1;
  }
  else
  {
    if (closestPoint)
    {
      double t;
      const double* pt4 = pts + 9;

      if (pcoords[0] < 0.0 && pcoords[1] < 0.0)
      {
        dist2 = vtkMath::Distance2BetweenPoints(x, pt1);
        for (int i = 0; i < 3; i++)
        {
          closestPoint[i] = pt1[i];
        }
      }
      else if (pcoords[0] > 1.0 && pcoords[1] < 0.0)
      {
        dist2 = vtkMath::Distance2BetweenPoints(x, pt2);
        for (int i = 0; i < 3; i++)
        {
          closestPoint[i] = pt2[i];
        }
      }
      else if (pcoords[0] > 1.0 && pcoords[1] > 1.0)
      {
        dist2 = vtkMath::Distance2BetweenPoints(x, pt3);
        for (int i = 0; i < 3; i++)
        {
          closestPoint[i] = pt3[i];
        }
      }
      else if (pcoords[0] < 0.0 && pcoords[1] > 1.0)
      {
        dist2 = vtkMath::Distance2BetweenPoints(x, pt4);
        for (int i = 0; i < 3; i++)
        {
          closestPoint[i] = pt4[i];
        }
      }
      else if (pcoords[0] < 0.0)
      {
        dist2 = vtkLine::DistanceToLine(x, pt1, pt4, t, closestPoint);
      }
      else if (pcoords[0] > 1.0)
      {
        dist2 = vtkLine::DistanceToLine(x, pt2, pt3, t, closestPoint);
      }
      else if (pcoords[1] < 0.0)
      {
        dist2 = vtkLine::DistanceToLine(x, pt1, pt2, t, closestPoint);
      }
      else if (pcoords[1] > 1.0)
      {
        dist2 = vtkLine::DistanceToLine(x, pt3, pt4, t, closestPoint);
      }
    }
    return 0;
  }
}

//------------------------------------------------------------------------------
void vtkQuad::EvaluateLocation(
  int& vtkNotUsed(subId), const double pcoords[3], double x[3], double* weights)
{

  vtkQuad::InterpolationFunctions(pcoords, weights);

  // Efficient point access
  const auto pointsArray = vtkDoubleArray::FastDownCast(this->Points->GetData());
  if (!pointsArray)
  {
    vtkErrorMacro(<< "Points should be double type");
    return;
  }
  const double* pts = pointsArray->GetPointer(0);

  x[0] = x[1] = x[2] = 0.0;
  for (int i = 0; i < 4; i++)
  {
    const double* pt = pts + 3 * i;
    for (int j = 0; j < 3; j++)
    {
      x[j] += pt[j] * weights[i];
    }
  }
}

//------------------------------------------------------------------------------
// Compute iso-parametric interpolation functions
//
void vtkQuad::InterpolationFunctions(const double pcoords[3], double sf[4])
{
  double rm = 1. - pcoords[0];
  double sm = 1. - pcoords[1];

  sf[0] = rm * sm;
  sf[1] = pcoords[0] * sm;
  sf[2] = pcoords[0] * pcoords[1];
  sf[3] = rm * pcoords[1];
}

//------------------------------------------------------------------------------
void vtkQuad::InterpolationDerivs(const double pcoords[3], double derivs[8])
{
  double rm = 1. - pcoords[0];
  double sm = 1. - pcoords[1];

  derivs[0] = -sm;
  derivs[1] = sm;
  derivs[2] = pcoords[1];
  derivs[3] = -pcoords[1];
  derivs[4] = -rm;
  derivs[5] = -pcoords[0];
  derivs[6] = pcoords[0];
  derivs[7] = rm;
}

//------------------------------------------------------------------------------
int vtkQuad::CellBoundary(int vtkNotUsed(subId), const double pcoords[3], vtkIdList* pts)
{
  double t1 = pcoords[0] - pcoords[1];
  double t2 = 1.0 - pcoords[0] - pcoords[1];

  pts->SetNumberOfIds(2);

  // compare against two lines in parametric space that divide element
  // into four pieces.
  if (t1 >= 0.0 && t2 >= 0.0)
  {
    pts->SetId(0, this->PointIds->GetId(0));
    pts->SetId(1, this->PointIds->GetId(1));
  }

  else if (t1 >= 0.0 && t2 < 0.0)
  {
    pts->SetId(0, this->PointIds->GetId(1));
    pts->SetId(1, this->PointIds->GetId(2));
  }

  else if (t1 < 0.0 && t2 < 0.0)
  {
    pts->SetId(0, this->PointIds->GetId(2));
    pts->SetId(1, this->PointIds->GetId(3));
  }

  else //( t1 < 0.0 && t2 >= 0.0 )
  {
    pts->SetId(0, this->PointIds->GetId(3));
    pts->SetId(1, this->PointIds->GetId(0));
  }

  if (pcoords[0] < 0.0 || pcoords[0] > 1.0 || pcoords[1] < 0.0 || pcoords[1] > 1.0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

//------------------------------------------------------------------------------
const vtkIdType* vtkQuad::GetEdgeArray(vtkIdType edgeId)
{
  return Edges[edgeId];
}

//------------------------------------------------------------------------------
void vtkQuad::Contour(double value, vtkDataArray* cellScalars, vtkIncrementalPointLocator* locator,
  vtkCellArray* verts, vtkCellArray* lines, vtkCellArray* vtkNotUsed(polys), vtkPointData* inPd,
  vtkPointData* outPd, vtkCellData* inCd, vtkIdType cellId, vtkCellData* outCd)
{
  static constexpr int CASE_MASK[4] = { 1, 2, 4, 8 };
  vtkIdType pts[2];
  int e1, e2;
  double t, x1[3], x2[3], x[3];
  vtkIdType offset = verts->GetNumberOfCells();

  // Build the case table
  int caseIndex = 0;
  for (int i = 0; i < 4; i++)
  {
    if (cellScalars->GetComponent(i, 0) >= value)
    {
      caseIndex |= CASE_MASK[i];
    }
  }

  const int* edge = vtkMarchingCellsContourCases::GetQuadCase(caseIndex);
  for (; edge[0] > -1; edge += 2)
  {
    for (int i = 0; i < 2; i++) // insert line
    {
      const vtkIdType* vert = Edges[edge[i]];
      // calculate a preferred interpolation direction
      double deltaScalar =
        (cellScalars->GetComponent(vert[1], 0) - cellScalars->GetComponent(vert[0], 0));
      if (deltaScalar > 0)
      {
        e1 = vert[0];
        e2 = vert[1];
      }
      else
      {
        e1 = vert[1];
        e2 = vert[0];
        deltaScalar = -deltaScalar;
      }

      // linear interpolation
      if (deltaScalar == 0.0)
      {
        t = 0.0;
      }
      else
      {
        t = (value - cellScalars->GetComponent(e1, 0)) / deltaScalar;
      }

      this->Points->GetPoint(e1, x1);
      this->Points->GetPoint(e2, x2);

      for (int j = 0; j < 3; j++)
      {
        x[j] = x1[j] + t * (x2[j] - x1[j]);
      }
      if (locator->InsertUniquePoint(x, pts[i]))
      {
        if (outPd)
        {
          vtkIdType p1 = this->PointIds->GetId(e1);
          vtkIdType p2 = this->PointIds->GetId(e2);
          outPd->InterpolateEdge(inPd, pts[i], p1, p2, t);
        }
      }
    }
    // check for degenerate line
    if (pts[0] != pts[1])
    {
      const vtkIdType newCellId = offset + lines->InsertNextCell(2, pts);
      if (outCd)
      {
        outCd->CopyData(inCd, cellId, newCellId);
      }
    }
  }
}

//------------------------------------------------------------------------------
vtkCell* vtkQuad::GetEdge(int edgeId)
{
  edgeId = std::clamp(edgeId, 0, 3);

  int edgeIdPlus1 = edgeId + 1;

  if (edgeIdPlus1 > 3)
  {
    edgeIdPlus1 = 0;
  }

  // load point id's
  this->Line->PointIds->SetId(0, this->PointIds->GetId(edgeId));
  this->Line->PointIds->SetId(1, this->PointIds->GetId(edgeIdPlus1));

  // load coordinates
  this->Line->Points->SetPoint(0, this->Points->GetPoint(edgeId));
  this->Line->Points->SetPoint(1, this->Points->GetPoint(edgeIdPlus1));

  return this->Line;
}

//------------------------------------------------------------------------------
// Intersect plane; see whether point is in quadrilateral. This code
// splits the quad into two triangles and intersects them (because the
// quad may be non-planar).
//
int vtkQuad::IntersectWithLine(const double p1[3], const double p2[3], double tol, double& t,
  double x[3], double pcoords[3], int& subId)
{
  int diagonalCase;
  double d1 = vtkMath::Distance2BetweenPoints(this->Points->GetPoint(0), this->Points->GetPoint(2));
  double d2 = vtkMath::Distance2BetweenPoints(this->Points->GetPoint(1), this->Points->GetPoint(3));
  subId = 0;

  // Figure out how to uniquely tessellate the quad. Watch out for
  // equivalent triangulations (i.e., the triangulation is equivalent
  // no matter where the diagonal). In this case use the point ids as
  // a tie breaker to ensure unique triangulation across the quad.
  //
  if (d1 == d2) // rare case; discriminate based on point id
  {
    int id, maxId = 0, maxIdx = 0;
    for (int i = 0; i < 4; i++) // find the maximum id
    {
      if ((id = this->PointIds->GetId(i)) > maxId)
      {
        maxId = id;
        maxIdx = i;
      }
    }
    if (maxIdx == 0 || maxIdx == 2)
    {
      diagonalCase = 0;
    }
    else
    {
      diagonalCase = 1;
    }
  }
  else if (d1 < d2)
  {
    diagonalCase = 0;
  }
  else // d2 < d1
  {
    diagonalCase = 1;
  }

  // Note: in the following code the parametric coords must be adjusted to
  // reflect the use of the triangle parametric coordinate system.
  IntersectionStruct res;
  switch (diagonalCase)
  {
    case 0:
    {
      this->Triangle->Points->SetPoint(0, this->Points->GetPoint(0));
      this->Triangle->Points->SetPoint(1, this->Points->GetPoint(1));
      this->Triangle->Points->SetPoint(2, this->Points->GetPoint(2));
      IntersectionStruct firstIntersect =
        IntersectionStruct::CellIntersectWithLine(this->Triangle, p1, p2, tol);

      this->Triangle->Points->SetPoint(0, this->Points->GetPoint(2));
      this->Triangle->Points->SetPoint(1, this->Points->GetPoint(3));
      this->Triangle->Points->SetPoint(2, this->Points->GetPoint(0));
      IntersectionStruct secondIntersect =
        IntersectionStruct::CellIntersectWithLine(this->Triangle, p1, p2, tol);

      bool useFirstIntersection = (firstIntersect && secondIntersect)
        ? (firstIntersect.T <= secondIntersect.T)
        : firstIntersect;
      bool useSecondIntersection = (firstIntersect && secondIntersect)
        ? (secondIntersect.T < firstIntersect.T)
        : secondIntersect;

      if (useFirstIntersection)
      {
        res = firstIntersect;
        res.PCoords[0] += res.PCoords[1];
      }
      else if (useSecondIntersection)
      {
        res = secondIntersect;
        res.PCoords[0] = 1.0 - (res.PCoords[0] + res.PCoords[1]);
        res.PCoords[1] = 1.0 - res.PCoords[1];
      }
    }
    break;

    case 1:
    {
      this->Triangle->Points->SetPoint(0, this->Points->GetPoint(0));
      this->Triangle->Points->SetPoint(1, this->Points->GetPoint(1));
      this->Triangle->Points->SetPoint(2, this->Points->GetPoint(3));
      IntersectionStruct firstIntersect =
        IntersectionStruct::CellIntersectWithLine(this->Triangle, p1, p2, tol);

      this->Triangle->Points->SetPoint(0, this->Points->GetPoint(2));
      this->Triangle->Points->SetPoint(1, this->Points->GetPoint(3));
      this->Triangle->Points->SetPoint(2, this->Points->GetPoint(1));
      IntersectionStruct secondIntersect =
        IntersectionStruct::CellIntersectWithLine(this->Triangle, p1, p2, tol);

      bool useFirstIntersection = (firstIntersect && secondIntersect)
        ? (firstIntersect.T <= secondIntersect.T)
        : firstIntersect;
      bool useSecondIntersection = (firstIntersect && secondIntersect)
        ? (secondIntersect.T < firstIntersect.T)
        : secondIntersect;

      if (useFirstIntersection)
      {
        res = firstIntersect;
      }
      else if (useSecondIntersection)
      {
        res = secondIntersect;
        res.PCoords[0] = 1.0 - res.PCoords[0];
        res.PCoords[1] = 1.0 - res.PCoords[1];
      }
    }
    break;
  }

  if (res)
  {
    res.CopyValues(t, x, pcoords, subId);
  }

  return res.Intersected;
}

//------------------------------------------------------------------------------
int vtkQuad::TriangulateLocalIds(int vtkNotUsed(index), vtkIdList* ptIds)
{
  // The base of the pyramid must be split into two triangles.  There are two
  // ways to do this (across either diagonal).  Pick the shorter diagonal.
  double d1 = vtkMath::Distance2BetweenPoints(this->Points->GetPoint(0), this->Points->GetPoint(2));
  double d2 = vtkMath::Distance2BetweenPoints(this->Points->GetPoint(1), this->Points->GetPoint(3));

  ptIds->SetNumberOfIds(6);
  if (d1 <= d2)
  {
    constexpr std::array<vtkIdType, 6> localPtIds{ 0, 1, 2, 0, 2, 3 };
    std::copy(localPtIds.begin(), localPtIds.end(), ptIds->begin());
  }
  else
  {
    constexpr std::array<vtkIdType, 6> localPtIds{ 0, 1, 3, 1, 2, 3 };
    std::copy(localPtIds.begin(), localPtIds.end(), ptIds->begin());
  }
  return 1;
}

//------------------------------------------------------------------------------
void vtkQuad::Derivatives(
  int vtkNotUsed(subId), const double pcoords[3], const double* values, int dim, double* derivs)
{
  double v0[2], v1[2], v2[2], v3[2], v10[3], v20[3], lenX;
  double x0[3], x1[3], x2[3], x3[3], n[3], vec20[3], vec30[3];
  double *J[2], J0[2], J1[2];
  double *JI[2], JI0[2], JI1[2];
  double funcDerivs[8], sum[2], dBydx, dBydy;

  // Project points of quad into 2D system
  this->Points->GetPoint(0, x0);
  this->Points->GetPoint(1, x1);
  this->Points->GetPoint(2, x2);
  ComputeNormal(this, x0, x1, x2, n);
  this->Points->GetPoint(3, x3);

  for (int i = 0; i < 3; i++)
  {
    v10[i] = x1[i] - x0[i];
    vec20[i] = x2[i] - x0[i];
    vec30[i] = x3[i] - x0[i];
  }

  vtkMath::Cross(n, v10, v20); // creates local y' axis

  if ((lenX = vtkMath::Normalize(v10)) <= 0.0 || vtkMath::Normalize(v20) <= 0.0) // degenerate
  {
    for (int j = 0; j < dim; j++)
    {
      for (int i = 0; i < 3; i++)
      {
        derivs[j * dim + i] = 0.0;
      }
    }
    return;
  }

  v0[0] = v0[1] = 0.0; // convert points to 2D (i.e., local system)
  v1[0] = lenX;
  v1[1] = 0.0;
  v2[0] = vtkMath::Dot(vec20, v10);
  v2[1] = vtkMath::Dot(vec20, v20);
  v3[0] = vtkMath::Dot(vec30, v10);
  v3[1] = vtkMath::Dot(vec30, v20);

  this->InterpolationDerivs(pcoords, funcDerivs);

  // Compute Jacobian and inverse Jacobian
  J[0] = J0;
  J[1] = J1;
  JI[0] = JI0;
  JI[1] = JI1;

  J[0][0] =
    v0[0] * funcDerivs[0] + v1[0] * funcDerivs[1] + v2[0] * funcDerivs[2] + v3[0] * funcDerivs[3];
  J[0][1] =
    v0[1] * funcDerivs[0] + v1[1] * funcDerivs[1] + v2[1] * funcDerivs[2] + v3[1] * funcDerivs[3];
  J[1][0] =
    v0[0] * funcDerivs[4] + v1[0] * funcDerivs[5] + v2[0] * funcDerivs[6] + v3[0] * funcDerivs[7];
  J[1][1] =
    v0[1] * funcDerivs[4] + v1[1] * funcDerivs[5] + v2[1] * funcDerivs[6] + v3[1] * funcDerivs[7];

  // Compute inverse Jacobian, return if Jacobian is singular
  if (!vtkMath::InvertMatrix(J, JI, 2))
  {
    for (int j = 0; j < dim; j++)
    {
      for (int i = 0; i < 3; i++)
      {
        derivs[j * dim + i] = 0.0;
      }
    }
    return;
  }

  // Loop over "dim" derivative values. For each set of values,
  // compute derivatives
  // in local system and then transform into modelling system.
  // First compute derivatives in local x'-y' coordinate system
  for (int j = 0; j < dim; j++)
  {
    sum[0] = sum[1] = 0.0;
    for (int i = 0; i < 4; i++) // loop over interp. function derivatives
    {
      sum[0] += funcDerivs[i] * values[dim * i + j];
      sum[1] += funcDerivs[4 + i] * values[dim * i + j];
    }
    dBydx = sum[0] * JI[0][0] + sum[1] * JI[0][1];
    dBydy = sum[0] * JI[1][0] + sum[1] * JI[1][1];

    // Transform into global system (dot product with global axes)
    derivs[3 * j] = dBydx * v10[0] + dBydy * v20[0];
    derivs[3 * j + 1] = dBydx * v10[1] + dBydy * v20[1];
    derivs[3 * j + 2] = dBydx * v10[2] + dBydy * v20[2];
  }
}

//------------------------------------------------------------------------------
// Clip this quad using scalar value provided. Like contouring, except
// that it cuts the quad to produce other quads and/or triangles.
void vtkQuad::Clip(double value, vtkDataArray* cellScalars, vtkIncrementalPointLocator* locator,
  vtkCellArray* polys, vtkPointData* inPd, vtkPointData* outPd, vtkCellData* inCd, vtkIdType cellId,
  vtkCellData* outCd, int insideOut)
{
  vtkIdType pts[4];
  double grdDiffs[4];
  double x[3], x1[3], x2[3];

  // Build the case table
  uint8_t caseIndex = 0;
  for (int pointId = 0; pointId < 4; ++pointId)
  {
    grdDiffs[pointId] = cellScalars->GetComponent(pointId, 0) - value;
    caseIndex |= (grdDiffs[pointId] >= 0.0) << pointId;
  }
  const uint8_t* thisCase = insideOut
    ? vtkMarchingCellsClipCases<true>::GetCellCase(VTK_QUAD, caseIndex)
    : vtkMarchingCellsClipCases<false>::GetCellCase(VTK_QUAD, caseIndex);
  using MCCases = vtkMarchingCellsClipCasesBase;
  const MCCases::EDGEIDXS* edgeVertices = MCCases::GetCellEdges(VTK_QUAD);
  const uint8_t numberOfOutputCells = *thisCase++;

  // generate each tri/quad
  for (uint8_t outputCellId = 0; outputCellId < numberOfOutputCells; ++outputCellId)
  {
    /*shape =*/thisCase++; // ST_TRI/ST_QUAD
    const uint8_t numberOfCellPoints = *thisCase++;
    for (int i = 0; i < numberOfCellPoints; ++i) // insert quad or triangle
    {
      const uint8_t pointIndex = *thisCase++;
      if (pointIndex <= MCCases::P7) // Input Point
      {
        this->Points->GetPoint(pointIndex, x);
        if (locator->InsertUniquePoint(x, pts[i]))
        {
          if (outPd)
          {
            outPd->CopyData(inPd, this->PointIds->GetId(pointIndex), pts[i]);
          }
        }
      }
      else // Mid-Edge Point
      {
        const auto& edgePoints = edgeVertices[pointIndex - MCCases::EA];
        uint8_t point1Index = edgePoints[0];
        uint8_t point2Index = edgePoints[1];
        double point1ToPoint2 = grdDiffs[point2Index] - grdDiffs[point1Index];
        if (point1ToPoint2 < 0)
        {
          std::swap(point1Index, point2Index);
          point1ToPoint2 = -point1ToPoint2;
        }
        const double point1ToIso = 0.0 - grdDiffs[point1Index];
        const double t = point1ToPoint2 != 0 ? point1ToIso / point1ToPoint2 : 0;
        this->Points->GetPoint(point1Index, x1);
        this->Points->GetPoint(point2Index, x2);

        for (int j = 0; j < 3; j++)
        {
          x[j] = x1[j] + t * (x2[j] - x1[j]);
        }

        if (locator->InsertUniquePoint(x, pts[i]))
        {
          if (outPd)
          {
            vtkIdType pointIndex1 = this->PointIds->GetId(point1Index);
            vtkIdType pointIndex2 = this->PointIds->GetId(point2Index);
            outPd->InterpolateEdge(inPd, pts[i], pointIndex1, pointIndex2, t);
          }
        }
      }
    }
    // check for degenerate output
    if (numberOfCellPoints == 3) // i.e., a triangle
    {
      if (pts[0] == pts[1] || pts[0] == pts[2] || pts[1] == pts[2])
      {
        continue;
      }
    }
    else // a quad
    {
      if ((pts[0] == pts[3] && pts[1] == pts[2]) || (pts[0] == pts[1] && pts[3] == pts[2]))
      {
        continue;
      }
    }

    const vtkIdType newCellId = polys->InsertNextCell(numberOfCellPoints, pts);
    outCd->CopyData(inCd, cellId, newCellId);
  }
}

//------------------------------------------------------------------------------
double* vtkQuad::GetParametricCoords()
{
  return ParametricCoords;
}

//------------------------------------------------------------------------------
void vtkQuad::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Line:\n";
  this->Line->PrintSelf(os, indent.GetNextIndent());
  os << indent << "Triangle:\n";
  this->Triangle->PrintSelf(os, indent.GetNextIndent());
}
VTK_ABI_NAMESPACE_END
