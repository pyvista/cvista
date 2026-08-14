// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
//.SECTION Thanks
// Thanks to Philippe Guerville who developed this class. <br>
// Thanks to Charles Pignerol (CEA-DAM, France) who ported this class under
// VTK 4.
// Thanks to Jean Favre (CSCS, Switzerland) who contributed to integrate this
// class in VTK. <br>
// Please address all comments to Jean Favre (jfavre at cscs.ch).
//
// The Interpolation functions and derivatives were changed in June
// 2015 by Bill Lorensen. These changes follow the formulation in:
// http://dilbert.engr.ucdavis.edu/~suku/nem/papers/polyelas.pdf
// NOTE: An additional copy of this paper is located at:
// http://www.vtk.org/Wiki/File:ApplicationOfPolygonalFiniteElementsInLinearElasticity.pdf

#include "vtkPentagonalPrism.h"

#include "vtkDoubleArray.h"
#include "vtkLine.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPoints.h"
#include "vtkPolygon.h"
#include "vtkQuad.h"
#include "vtkTriangle.h"

#include <algorithm> //std::copy
#include <cassert>

namespace
{

//------------------------------------------------------------------------------
[[maybe_unused]] constexpr const char* Topology = R"(
   Pentagonal Prism topology:

                       3
                      /|\
                     / | \
                    /  |  \
                   /   |   \
                  /    |    \
                 /     |     \
                /      |      \
               /       8       \
              /       / \       \
             /       /   \       \
            /       /     \       \
           /       /       \       \
          /       /         \       \
         /       /           \       \
        /       /             \       \
       /       /               \       \
      /       /                 \       \
     4-------9                   7-------2
      \       \                 /       /
       \       \               /       /
        \       \             /       /
         \       5-----------6       /
          \     /             \     /
           \   /               \   /
            \ /                 \ /
             0-------------------1
)";

//------------------------------------------------------------------------------
// See: http://dilbert.engr.ucdavis.edu/~suku/nem/papers/polyelas.pdf
double ParametricCoords[30] = {
  0.654508, 0.975528, 0,  //
  0.0954915, 0.793893, 0, //
  0.0954915, 0.206107, 0, //
  0.654508, 0.0244717, 0, //
  1, 0.5, 0,              //
  0.654508, 0.975528, 1,  //
  0.0954915, 0.793893, 1, //
  0.0954915, 0.206107, 1, //
  0.654508, 0.0244717, 1, //
  1, 0.5, 1               //
};

//------------------------------------------------------------------------------
constexpr vtkIdType Edges[vtkPentagonalPrism::NumberOfEdges][2] = {
  { 0, 1 }, // 0
  { 1, 2 }, // 1
  { 2, 3 }, // 2
  { 3, 4 }, // 3
  { 4, 0 }, // 4
  { 5, 6 }, // 5
  { 6, 7 }, // 6
  { 7, 8 }, // 7
  { 8, 9 }, // 8
  { 9, 5 }, // 9
  { 0, 5 }, // 10
  { 1, 6 }, // 11
  { 2, 7 }, // 12
  { 3, 8 }, // 13
  { 4, 9 }, // 14
};

//------------------------------------------------------------------------------
constexpr vtkIdType Faces[vtkPentagonalPrism::NumberOfFaces]
                         [vtkPentagonalPrism::MaximumFaceSize + 1] = {
                           { 0, 4, 3, 2, 1, -1 },  // 0
                           { 5, 6, 7, 8, 9, -1 },  // 1
                           { 0, 1, 6, 5, -1, -1 }, // 2
                           { 1, 2, 7, 6, -1, -1 }, // 3
                           { 2, 3, 8, 7, -1, -1 }, // 4
                           { 3, 4, 9, 8, -1, -1 }, // 5
                           { 4, 0, 5, 9, -1, -1 }, // 6
                         };

//------------------------------------------------------------------------------
constexpr vtkIdType EdgeToAdjacentFaces[vtkPentagonalPrism::NumberOfEdges][2] = {
  { 0, 2 }, // 0
  { 0, 3 }, // 1
  { 0, 4 }, // 2
  { 0, 5 }, // 3
  { 0, 6 }, // 4
  { 1, 2 }, // 5
  { 1, 3 }, // 6
  { 1, 4 }, // 7
  { 1, 5 }, // 8
  { 1, 6 }, // 9
  { 2, 6 }, // 10
  { 2, 3 }, // 11
  { 3, 4 }, // 12
  { 4, 5 }, // 13
  { 5, 6 }, // 14
};

//------------------------------------------------------------------------------
constexpr vtkIdType FaceToAdjacentFaces[vtkPentagonalPrism::NumberOfFaces]
                                       [vtkPentagonalPrism::MaximumFaceSize] = {
                                         { 6, 5, 4, 3, 2 },  // 0
                                         { 2, 3, 4, 5, 6 },  // 1
                                         { 0, 3, 1, 6, -1 }, // 2
                                         { 0, 4, 1, 2, -1 }, // 3
                                         { 0, 5, 1, 3, -1 }, // 4
                                         { 0, 6, 1, 4, -1 }, // 5
                                         { 0, 2, 1, 5, -1 }, // 6
                                       };

//------------------------------------------------------------------------------
constexpr vtkIdType PointToIncidentEdges[vtkPentagonalPrism::NumberOfPoints]
                                        [vtkPentagonalPrism::MaximumValence] = {
                                          { 0, 10, 4 }, // 0
                                          { 0, 1, 11 }, // 1
                                          { 1, 2, 12 }, // 2
                                          { 2, 3, 13 }, // 3
                                          { 3, 4, 14 }, // 4
                                          { 5, 9, 10 }, // 5
                                          { 5, 11, 6 }, // 6
                                          { 6, 12, 7 }, // 7
                                          { 7, 13, 8 }, // 8
                                          { 8, 14, 9 }, // 9
                                        };

//------------------------------------------------------------------------------
constexpr vtkIdType PointToIncidentFaces[vtkPentagonalPrism::NumberOfPoints]
                                        [vtkPentagonalPrism::MaximumValence] = {
                                          { 2, 6, 0 }, // 0
                                          { 0, 3, 2 }, // 1
                                          { 0, 4, 3 }, // 2
                                          { 0, 5, 4 }, // 3
                                          { 0, 6, 5 }, // 4
                                          { 1, 6, 2 }, // 5
                                          { 2, 3, 1 }, // 6
                                          { 3, 4, 1 }, // 7
                                          { 4, 5, 1 }, // 8
                                          { 5, 6, 1 }, // 9
                                        };

//------------------------------------------------------------------------------
constexpr vtkIdType PointToOneRingPoints[vtkPentagonalPrism::NumberOfPoints]
                                        [vtkPentagonalPrism::MaximumValence] = {
                                          { 1, 5, 4 }, // 0
                                          { 0, 2, 6 }, // 1
                                          { 1, 3, 7 }, // 2
                                          { 2, 4, 8 }, // 3
                                          { 3, 0, 9 }, // 4
                                          { 6, 9, 0 }, // 5
                                          { 5, 1, 7 }, // 6
                                          { 6, 2, 8 }, // 7
                                          { 7, 3, 9 }, // 8
                                          { 8, 4, 5 }, // 9
                                        };

//------------------------------------------------------------------------------
constexpr vtkIdType NumberOfPointsInFace[vtkPentagonalPrism::NumberOfFaces] = {
  5, // 0
  5, // 1
  4, // 2
  4, // 3
  4, // 4
  4, // 5
  4  // 6
};

constexpr double VTK_DIVERGED = 1.e6;
constexpr int VTK_MAX_ITERATIONS = 20;
constexpr double VTK_CONVERGED = 1.e-04;
}

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkPentagonalPrism);

//------------------------------------------------------------------------------
// Construct the prism with ten points.
vtkPentagonalPrism::vtkPentagonalPrism()
{
  this->Points->SetNumberOfPoints(10);
  this->PointIds->SetNumberOfIds(10);
  for (int i = 0; i < 10; i++)
  {
    this->Points->SetPoint(i, 0.0, 0.0, 0.0);
    this->PointIds->SetId(i, 0);
  }

  this->Line = vtkSmartPointer<vtkLine>::New();
  this->Quad = vtkSmartPointer<vtkQuad>::New();
  this->Triangle = vtkSmartPointer<vtkTriangle>::New();
  this->Polygon = vtkSmartPointer<vtkPolygon>::New();
  this->Polygon->PointIds->SetNumberOfIds(5);
  this->Polygon->Points->SetNumberOfPoints(5);

  for (int i = 0; i < 5; i++)
  {
    this->Polygon->Points->SetPoint(i, 0.0, 0.0, 0.0);
    this->Polygon->PointIds->SetId(i, 0);
  }
}

//------------------------------------------------------------------------------
int vtkPentagonalPrism::EvaluatePosition(const double x[3], double closestPoint[3], int& subId,
  double pcoords[3], double& dist2, double weights[])
{
  int converged;
  double params[3];
  double fcol[3], rcol[3], scol[3], tcol[3];
  double derivs[30];

  // Efficient point access
  const auto pointsArray = vtkDoubleArray::FastDownCast(this->Points->GetData());
  if (!pointsArray)
  {
    vtkErrorMacro(<< "Points should be double type");
    return 0;
  }
  const double* pts = pointsArray->GetPointer(0);

  // set initial position for Newton's method
  subId = 0;
  pcoords[0] = pcoords[1] = pcoords[2] = params[0] = params[1] = params[2] = 0.5;

  //  enter iteration loop
  for (int iteration = converged = 0; !converged && iteration < VTK_MAX_ITERATIONS; iteration++)
  {
    //  calculate element interpolation functions and derivatives
    vtkPentagonalPrism::InterpolationFunctions(pcoords, weights);
    vtkPentagonalPrism::InterpolationDerivs(pcoords, derivs);

    //  calculate newton functions
    for (int i = 0; i < 3; i++)
    {
      fcol[i] = rcol[i] = scol[i] = tcol[i] = 0.0;
    }
    for (int i = 0; i < 10; i++)
    {
      const double* pt = pts + 3 * i;
      for (int j = 0; j < 3; j++)
      {
        fcol[j] += pt[j] * weights[i];
        rcol[j] += pt[j] * derivs[i];
        scol[j] += pt[j] * derivs[i + 10];
        tcol[j] += pt[j] * derivs[i + 20];
      }
    }

    for (int i = 0; i < 3; i++)
    {
      fcol[i] -= x[i];
    }

    //  compute determinants and generate improvements
    double d = vtkMath::Determinant3x3(rcol, scol, tcol);
    if (std::abs(d) < 1.e-20)
    {
      vtkDebugMacro(<< "Determinant incorrect, iteration " << iteration);
      return -1;
    }

    pcoords[0] = params[0] - vtkMath::Determinant3x3(fcol, scol, tcol) / d;
    pcoords[1] = params[1] - vtkMath::Determinant3x3(rcol, fcol, tcol) / d;
    pcoords[2] = params[2] - vtkMath::Determinant3x3(rcol, scol, fcol) / d;

    //  check for convergence
    if (std::abs(pcoords[0] - params[0]) < VTK_CONVERGED &&
      std::abs(pcoords[1] - params[1]) < VTK_CONVERGED &&
      std::abs(pcoords[2] - params[2]) < VTK_CONVERGED)
    {
      converged = 1;
    }
    // Test for bad divergence (S.Hirschberg 11.12.2001)
    else if (std::abs(pcoords[0]) > VTK_DIVERGED || std::abs(pcoords[1]) > VTK_DIVERGED ||
      std::abs(pcoords[2]) > VTK_DIVERGED)
    {
      return -1;
    }
    //  if not converged, repeat
    else
    {
      params[0] = pcoords[0];
      params[1] = pcoords[1];
      params[2] = pcoords[2];
    }
  }

  //  if not converged, set the parametric coordinates to arbitrary values
  //  outside of element
  if (!converged)
  {
    return -1;
  }

  vtkPentagonalPrism::InterpolationFunctions(pcoords, weights);

  if (pcoords[0] >= -0.001 && pcoords[0] <= 1.001 && pcoords[1] >= -0.001 && pcoords[1] <= 1.001 &&
    pcoords[2] >= -0.001 && pcoords[2] <= 1.001)
  {
    if (closestPoint)
    {
      closestPoint[0] = x[0];
      closestPoint[1] = x[1];
      closestPoint[2] = x[2];
      dist2 = 0.0; // inside hexahedron
    }
    return 1;
  }
  else
  {
    double pc[3], w[10];
    if (closestPoint)
    {
      for (int i = 0; i < 3; i++) // only approximate, not really true for warped hexa
      {
        if (pcoords[i] < 0.0)
        {
          pc[i] = 0.0;
        }
        else if (pcoords[i] > 1.0)
        {
          pc[i] = 1.0;
        }
        else
        {
          pc[i] = pcoords[i];
        }
      }
      this->EvaluateLocation(subId, pc, closestPoint, static_cast<double*>(w));
      dist2 = vtkMath::Distance2BetweenPoints(closestPoint, x);
    }
    return 0;
  }
}

//------------------------------------------------------------------------------
//
// Compute iso-parametric interpolation functions
// See:
// http://dilbert.engr.ucdavis.edu/~suku/nem/papers/polyelas.pdf
void vtkPentagonalPrism::InterpolationFunctions(const double pcoords[3], double weights[10])
{
  // VTK needs parametric coordinates to be between [0,1]. Isoparametric
  // shape functions are formulated between [-1,1]. Here we do a
  // coordinate system conversion from [0,1] to [-1,1].
  double x = 2.0 * (pcoords[0] - 0.5);
  double y = 2.0 * (pcoords[1] - 0.5);
  double z = pcoords[2]; // z is from 0 to 1

  // From Appendix A.1 Pentagonal reference element (n = 5)
  double b = 87.05 - 12.7004 * x * x - 12.7004 * y * y;

  double a[5];
  a[0] = -0.092937 * (3.23607 + 4 * x) * (-3.80423 + 3.80423 * x - 2.76393 * y) *
    (15.2169 + 5.81234 * x + 17.8885 * y);
  a[1] = -0.0790569 * (3.80423 - 3.80423 * x - 2.76393 * y) *
    (-3.80423 + 3.80423 * x - 2.76393 * y) * (15.2169 + 5.81234 * x + 17.8885 * y);
  a[2] = -0.0790569 * (15.2169 + 5.81234 * x - 17.8885 * y) *
    (3.80423 - 3.80423 * x - 2.76393 * y) * (-3.80423 + 3.80423 * x - 2.76393 * y);
  a[3] = 0.092937 * (3.23607 + 4.0 * x) * (15.2169 + 5.81234 * x - 17.8885 * y) *
    (3.80423 - 3.80423 * x - 2.76393 * y);
  a[4] = 0.0232343 * (3.23607 + 4.0 * x) * (15.2169 + 5.81234 * x - 17.8885 * y) *
    (15.2169 + 5.81234 * x + 17.8885 * y);

  for (int i = 0; i < 5; ++i)
  {
    weights[i] = -(a[i] / b) * (z - 1.0);
    weights[i + 5] = (a[i] / b) * (z - 0.0);
  }
}

//------------------------------------------------------------------------------
//
// Compute iso-parametric interpolation derivatives
// See:
// http://dilbert.engr.ucdavis.edu/~suku/nem/papers/polyelas.pdf
//
void vtkPentagonalPrism::InterpolationDerivs(const double pcoords[3], double derivs[30])
{
  // VTK needs parametric coordinates to be between [0,1]. Isoparametric
  // shape functions are formulated between [-1,1]. Here we do a
  // coordinate system conversion from [0,1] to [-1,1].
  double x = 2.0 * (pcoords[0] - 0.5);
  double y = 2.0 * (pcoords[1] - 0.5);
  double z = pcoords[2]; // z is from 0 to 1

  double dd[20];

  // x-derivatives
  // First pentagon
  double x2 = x * x;
  double y2 = y * y;
  double denom = (-12.7004 * x2 - 12.7004 * y2 + 87.05);
  double denom2 = denom * denom;

  // Please excuse the line length. This code was generated using the
  // symbolic math package SymPy. (http://www.sympy.org)

  dd[0] = 25.4008 * x * (-0.371748 * x - 0.30075063759) * (3.80423 * x - 2.76393 * y - 3.80423) *
      (5.81234 * x + 17.8885 * y + 15.2169) / denom2 +
    5.81234 * (-0.371748 * x - 0.30075063759) * (3.80423 * x - 2.76393 * y - 3.80423) / denom +
    3.80423 * (-0.371748 * x - 0.30075063759) * (5.81234 * x + 17.8885 * y + 15.2169) / denom -
    0.371748 * (3.80423 * x - 2.76393 * y - 3.80423) * (5.81234 * x + 17.8885 * y + 15.2169) /
      denom;

  dd[1] = 25.4008 * x * (0.300750630687 * x + 0.218507737617 * y - 0.300750630687) *
      (3.80423 * x - 2.76393 * y - 3.80423) * (5.81234 * x + 17.8885 * y + 15.2169) / denom2 +
    5.81234 * (0.300750630687 * x + 0.218507737617 * y - 0.300750630687) *
      (3.80423 * x - 2.76393 * y - 3.80423) / denom +
    3.80423 * (0.300750630687 * x + 0.218507737617 * y - 0.300750630687) *
      (5.81234 * x + 17.8885 * y + 15.2169) / denom +
    0.300750630687 * (3.80423 * x - 2.76393 * y - 3.80423) * (5.81234 * x + 17.8885 * y + 15.2169) /
      denom;

  dd[2] = 25.4008 * x * (-3.80423 * x - 2.76393 * y + 3.80423) *
      (-0.459505582146 * x + 1.41420935565 * y - 1.20300094161) *
      (3.80423 * x - 2.76393 * y - 3.80423) / denom2 +
    3.80423 * (-3.80423 * x - 2.76393 * y + 3.80423) *
      (-0.459505582146 * x + 1.41420935565 * y - 1.20300094161) / denom -
    0.459505582146 * (-3.80423 * x - 2.76393 * y + 3.80423) *
      (3.80423 * x - 2.76393 * y - 3.80423) / denom -
    3.80423 * (-0.459505582146 * x + 1.41420935565 * y - 1.20300094161) *
      (3.80423 * x - 2.76393 * y - 3.80423) / denom;

  dd[3] = 25.4008 * x * (0.371748 * x + 0.30075063759) * (-3.80423 * x - 2.76393 * y + 3.80423) *
      (5.81234 * x - 17.8885 * y + 15.2169) / denom2 +
    5.81234 * (0.371748 * x + 0.30075063759) * (-3.80423 * x - 2.76393 * y + 3.80423) / denom -
    3.80423 * (0.371748 * x + 0.30075063759) * (5.81234 * x - 17.8885 * y + 15.2169) / denom +
    0.371748 * (-3.80423 * x - 2.76393 * y + 3.80423) * (5.81234 * x - 17.8885 * y + 15.2169) /
      denom;

  dd[4] = 25.4008 * x * (0.0929372 * x + 0.075187821201) * (5.81234 * x - 17.8885 * y + 15.2169) *
      (5.81234 * x + 17.8885 * y + 15.2169) / denom2 +
    5.81234 * (0.0929372 * x + 0.075187821201) * (5.81234 * x - 17.8885 * y + 15.2169) / denom +
    5.81234 * (0.0929372 * x + 0.075187821201) * (5.81234 * x + 17.8885 * y + 15.2169) / denom +
    0.0929372 * (5.81234 * x - 17.8885 * y + 15.2169) * (5.81234 * x + 17.8885 * y + 15.2169) /
      denom;

  // y-derivatives
  // First pentagon
  dd[10] = 25.4008 * y * (-0.371748 * x - 0.30075063759) * (3.80423 * x - 2.76393 * y - 3.80423) *
      (5.81234 * x + 17.8885 * y + 15.2169) / denom2 +
    17.8885 * (-0.371748 * x - 0.30075063759) * (3.80423 * x - 2.76393 * y - 3.80423) / denom -
    2.76393 * (-0.371748 * x - 0.30075063759) * (5.81234 * x + 17.8885 * y + 15.2169) / denom;

  dd[11] = 25.4008 * y * (0.300750630687 * x + 0.218507737617 * y - 0.300750630687) *
      (3.80423 * x - 2.76393 * y - 3.80423) * (5.81234 * x + 17.8885 * y + 15.2169) / denom2 +
    17.8885 * (0.300750630687 * x + 0.218507737617 * y - 0.300750630687) *
      (3.80423 * x - 2.76393 * y - 3.80423) / denom -
    2.76393 * (0.300750630687 * x + 0.218507737617 * y - 0.300750630687) *
      (5.81234 * x + 17.8885 * y + 15.2169) / denom +
    0.218507737617 * (3.80423 * x - 2.76393 * y - 3.80423) * (5.81234 * x + 17.8885 * y + 15.2169) /
      denom;

  dd[12] = 25.4008 * y * (-3.80423 * x - 2.76393 * y + 3.80423) *
      (-0.459505582146 * x + 1.41420935565 * y - 1.20300094161) *
      (3.80423 * x - 2.76393 * y - 3.80423) / denom2 -
    2.76393 * (-3.80423 * x - 2.76393 * y + 3.80423) *
      (-0.459505582146 * x + 1.41420935565 * y - 1.20300094161) / denom +
    1.41420935565 * (-3.80423 * x - 2.76393 * y + 3.80423) * (3.80423 * x - 2.76393 * y - 3.80423) /
      denom -
    2.76393 * (-0.459505582146 * x + 1.41420935565 * y - 1.20300094161) *
      (3.80423 * x - 2.76393 * y - 3.80423) / denom;

  dd[13] = 25.4008 * y * (0.371748 * x + 0.30075063759) * (-3.80423 * x - 2.76393 * y + 3.80423) *
      (5.81234 * x - 17.8885 * y + 15.2169) / denom2 -
    17.8885 * (0.371748 * x + 0.30075063759) * (-3.80423 * x - 2.76393 * y + 3.80423) / denom -
    2.76393 * (0.371748 * x + 0.30075063759) * (5.81234 * x - 17.8885 * y + 15.2169) / denom;

  dd[14] = 25.4008 * y * (0.0929372 * x + 0.075187821201) * (5.81234 * x - 17.8885 * y + 15.2169) *
      (5.81234 * x + 17.8885 * y + 15.2169) / denom2 +
    17.8885 * (0.0929372 * x + 0.075187821201) * (5.81234 * x - 17.8885 * y + 15.2169) / denom -
    17.8885 * (0.0929372 * x + 0.075187821201) * (5.81234 * x + 17.8885 * y + 15.2169) / denom;

  // z-derivatives
  // First pentagon
  double b = 87.05 - 12.7004 * x * x - 12.7004 * y * y;
  dd[15] = -0.092937 * (3.23607 + 4 * x) * (-3.80423 + 3.80423 * x - 2.76393 * y) *
    (15.2169 + 5.81234 * x + 17.8885 * y) / b;
  dd[16] = -0.0790569 * (3.80423 - 3.80423 * x - 2.76393 * y) *
    (-3.80423 + 3.80423 * x - 2.76393 * y) * (15.2169 + 5.81234 * x + 17.8885 * y) / b;
  dd[17] = -0.0790569 * (15.2169 + 5.81234 * x - 17.8885 * y) *
    (3.80423 - 3.80423 * x - 2.76393 * y) * (-3.80423 + 3.80423 * x - 2.76393 * y) / b;
  dd[18] = 0.092937 * (3.23607 + 4.0 * x) * (15.2169 + 5.81234 * x - 17.8885 * y) *
    (3.80423 - 3.80423 * x - 2.76393 * y) / b;
  dd[19] = 0.0232343 * (3.23607 + 4.0 * x) * (15.2169 + 5.81234 * x - 17.8885 * y) *
    (15.2169 + 5.81234 * x + 17.8885 * y) / b;

  for (int i = 0; i < 5; ++i)
  {
    derivs[i] = -dd[i] * (z - 1.0);           // x deriv first pentagon
    derivs[i + 5] = dd[i] * (z + 0.0);        // x deriv second pentagon
    derivs[i + 10] = -dd[i + 10] * (z - 1.0); // y deriv first pentagon
    derivs[i + 15] = dd[i + 10] * (z + 0.0);  // y deriv second pentagon
    derivs[i + 20] = -dd[i + 15];             // z deriv first pentagon
    derivs[i + 25] = dd[i + 15];              // z deriv second pentagon
  }

  // We compute derivatives in [-1; 1] but we need them in [ 0; 1]
  for (int i = 0; i < 30; i++)
  {
    derivs[i] *= 2;
  }
}

//------------------------------------------------------------------------------
void vtkPentagonalPrism::EvaluateLocation(
  int& vtkNotUsed(subId), const double pcoords[3], double x[3], double* weights)
{
  this->InterpolationFunctions(pcoords, weights);

  // Efficient point access
  const auto pointsArray = vtkDoubleArray::FastDownCast(this->Points->GetData());
  if (!pointsArray)
  {
    vtkErrorMacro(<< "Points should be double type");
    return;
  }
  const double* pts = pointsArray->GetPointer(0);

  x[0] = x[1] = x[2] = 0.0;
  for (int i = 0; i < 10; i++)
  {
    const double* pt = pts + 3 * i;
    for (int j = 0; j < 3; j++)
    {
      x[j] += pt[j] * weights[i];
    }
  }
}

//------------------------------------------------------------------------------
bool vtkPentagonalPrism::GetCentroid(double centroid[3]) const
{
  return vtkPentagonalPrism::ComputeCentroid(this->Points, nullptr, centroid);
}

//------------------------------------------------------------------------------
bool vtkPentagonalPrism::ComputeCentroid(
  vtkPoints* points, const vtkIdType* pointIds, double centroid[3])
{
  double p[3];
  if (!pointIds)
  {
    vtkPolygon::ComputeCentroid(points, NumberOfPointsInFace[0], Faces[0], centroid);
    vtkPolygon::ComputeCentroid(points, NumberOfPointsInFace[1], Faces[1], p);
  }
  else
  {
    vtkIdType facePointsIds[5] = { pointIds[Faces[0][0]], pointIds[Faces[0][1]],
      pointIds[Faces[0][2]], pointIds[Faces[0][3]], pointIds[Faces[0][4]] };
    vtkPolygon::ComputeCentroid(points, NumberOfPointsInFace[0], facePointsIds, centroid);
    facePointsIds[0] = pointIds[Faces[1][0]];
    facePointsIds[1] = pointIds[Faces[1][1]];
    facePointsIds[2] = pointIds[Faces[1][2]];
    facePointsIds[3] = pointIds[Faces[1][3]];
    facePointsIds[4] = pointIds[Faces[1][4]];
    vtkPolygon::ComputeCentroid(points, NumberOfPointsInFace[1], facePointsIds, p);
  }
  centroid[0] += p[0];
  centroid[1] += p[1];
  centroid[2] += p[2];
  centroid[0] *= 0.5;
  centroid[1] *= 0.5;
  centroid[2] *= 0.5;
  return true;
}

//------------------------------------------------------------------------------
bool vtkPentagonalPrism::IsInsideOut()
{
  double n0[3], n1[3];
  vtkPolygon::ComputeNormal(this->Points, NumberOfPointsInFace[0], Faces[0], n0);
  vtkPolygon::ComputeNormal(this->Points, NumberOfPointsInFace[1], Faces[1], n1);
  return vtkMath::Dot(n0, n1) > 0.0;
}

//------------------------------------------------------------------------------
// Returns the closest face to the point specified. Closeness is measured
// parametrically.
int vtkPentagonalPrism::CellBoundary(int subId, const double pcoords[3], vtkIdList* pts)
{
  // load coordinates
  double* points = this->GetParametricCoords();
  for (int i = 0; i < 5; i++)
  {
    this->Polygon->PointIds->SetId(i, i);
    this->Polygon->Points->SetPoint(i, &points[3 * i]);
  }

  this->Polygon->CellBoundary(subId, pcoords, pts);

  int min = vtkMath::Min(pts->GetId(0), pts->GetId(1));
  int max = vtkMath::Max(pts->GetId(0), pts->GetId(1));

  // Base on the edge find the quad that correspond:
  int index;
  if ((index = (max - min)) > 1)
  {
    index = 6;
  }
  else
  {
    index += min + 1;
  }

  double a[3], b[3], u[3], v[3];
  this->Polygon->Points->GetPoint(pts->GetId(0), a);
  this->Polygon->Points->GetPoint(pts->GetId(1), b);
  u[0] = b[0] - a[0];
  u[1] = b[1] - a[1];
  v[0] = pcoords[0] - a[0];
  v[1] = pcoords[1] - a[1];

  double dot = vtkMath::Dot2D(v, u);
  if (double uNorm = vtkMath::Norm2D(u))
  {
    dot /= uNorm;
  }
  dot = (v[0] * v[0] + v[1] * v[1]) - dot * dot;
  // mathematically dot must be >= zero but, surprise surprise, it can actually
  // be negative
  if (dot > 0)
  {
    dot = sqrt(dot);
  }
  else
  {
    dot = 0;
  }

  if (pcoords[2] < 0.5)
  {
    // could be closer to face 1
    // compare that distance to the distance to the quad.

    if (dot < pcoords[2])
    {
      // We are closer to the quad face
      const vtkIdType* verts = Faces[index];
      for (int i = 0; i < 4; i++)
      {
        pts->InsertId(i, verts[i]);
      }
    }
    else
    {
      // we are closer to the penta face 1
      for (int i = 0; i < 5; i++)
      {
        pts->InsertId(i, Faces[0][i]);
      }
    }
  }
  else
  {
    // could be closer to face 2
    // compare that distance to the distance to the quad.

    if (dot < (1. - pcoords[2]))
    {
      // We are closer to the quad face
      const vtkIdType* verts = Faces[index];
      for (int i = 0; i < 4; i++)
      {
        pts->InsertId(i, verts[i]);
      }
    }
    else
    {
      // we are closer to the penta face 2
      for (int i = 0; i < 5; i++)
      {
        pts->InsertId(i, Faces[1][i]);
      }
    }
  }

  // determine whether point is inside of hexagon
  if (pcoords[0] < 0.0 || pcoords[0] > 1.0 || pcoords[1] < 0.0 || pcoords[1] > 1.0 ||
    pcoords[2] < 0.0 || pcoords[2] > 1.0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

//------------------------------------------------------------------------------
vtkCell* vtkPentagonalPrism::GetEdge(int edgeId)
{
  const vtkIdType* verts = Edges[edgeId];

  // load point id's
  this->Line->PointIds->SetId(0, this->PointIds->GetId(verts[0]));
  this->Line->PointIds->SetId(1, this->PointIds->GetId(verts[1]));

  // load coordinates
  this->Line->Points->SetPoint(0, this->Points->GetPoint(verts[0]));
  this->Line->Points->SetPoint(1, this->Points->GetPoint(verts[1]));

  return this->Line;
}

//------------------------------------------------------------------------------
vtkCell* vtkPentagonalPrism::GetFace(int faceId)
{
  const vtkIdType* verts = Faces[faceId];

  if (verts[4] != -1) // polys cell
  {
    // load point id's
    this->Polygon->PointIds->SetId(0, this->PointIds->GetId(verts[0]));
    this->Polygon->PointIds->SetId(1, this->PointIds->GetId(verts[1]));
    this->Polygon->PointIds->SetId(2, this->PointIds->GetId(verts[2]));
    this->Polygon->PointIds->SetId(3, this->PointIds->GetId(verts[3]));
    this->Polygon->PointIds->SetId(4, this->PointIds->GetId(verts[4]));

    // load coordinates
    this->Polygon->Points->SetPoint(0, this->Points->GetPoint(verts[0]));
    this->Polygon->Points->SetPoint(1, this->Points->GetPoint(verts[1]));
    this->Polygon->Points->SetPoint(2, this->Points->GetPoint(verts[2]));
    this->Polygon->Points->SetPoint(3, this->Points->GetPoint(verts[3]));
    this->Polygon->Points->SetPoint(4, this->Points->GetPoint(verts[4]));

    return this->Polygon;
  }
  else
  {
    // load point id's
    this->Quad->PointIds->SetId(0, this->PointIds->GetId(verts[0]));
    this->Quad->PointIds->SetId(1, this->PointIds->GetId(verts[1]));
    this->Quad->PointIds->SetId(2, this->PointIds->GetId(verts[2]));
    this->Quad->PointIds->SetId(3, this->PointIds->GetId(verts[3]));

    // load coordinates
    this->Quad->Points->SetPoint(0, this->Points->GetPoint(verts[0]));
    this->Quad->Points->SetPoint(1, this->Points->GetPoint(verts[1]));
    this->Quad->Points->SetPoint(2, this->Points->GetPoint(verts[2]));
    this->Quad->Points->SetPoint(3, this->Points->GetPoint(verts[3]));

    return this->Quad;
  }
}

//------------------------------------------------------------------------------
const vtkIdType* vtkPentagonalPrism::GetEdgeArray(vtkIdType edgeId)
{
  assert(edgeId < vtkPentagonalPrism::NumberOfEdges && "edgeId too large");
  return Edges[edgeId];
}

//------------------------------------------------------------------------------
const vtkIdType* vtkPentagonalPrism::GetFaceArray(vtkIdType faceId)
{
  assert(faceId < vtkPentagonalPrism::NumberOfFaces && "faceId too large");
  return Faces[faceId];
}

//------------------------------------------------------------------------------
const vtkIdType* vtkPentagonalPrism::GetEdgeToAdjacentFacesArray(vtkIdType edgeId)
{
  assert(edgeId < vtkPentagonalPrism::NumberOfEdges && "edgeId too large");
  return EdgeToAdjacentFaces[edgeId];
}

//------------------------------------------------------------------------------
const vtkIdType* vtkPentagonalPrism::GetFaceToAdjacentFacesArray(vtkIdType faceId)
{
  assert(faceId < vtkPentagonalPrism::NumberOfFaces && "faceId too large");
  return FaceToAdjacentFaces[faceId];
}

//------------------------------------------------------------------------------
const vtkIdType* vtkPentagonalPrism::GetPointToIncidentEdgesArray(vtkIdType pointId)
{
  assert(pointId < vtkPentagonalPrism::NumberOfPoints && "pointId too large");
  return PointToIncidentEdges[pointId];
}

//------------------------------------------------------------------------------
const vtkIdType* vtkPentagonalPrism::GetPointToIncidentFacesArray(vtkIdType pointId)
{
  assert(pointId < vtkPentagonalPrism::NumberOfPoints && "pointId too large");
  return PointToIncidentFaces[pointId];
}

//------------------------------------------------------------------------------
const vtkIdType* vtkPentagonalPrism::GetPointToOneRingPointsArray(vtkIdType pointId)
{
  assert(pointId < vtkPentagonalPrism::NumberOfPoints && "pointId too large");
  return PointToOneRingPoints[pointId];
}

//------------------------------------------------------------------------------
void vtkPentagonalPrism::GetEdgePoints(vtkIdType edgeId, const vtkIdType*& pts)
{
  assert(edgeId < vtkPentagonalPrism::NumberOfEdges && "edgeId too large");
  pts = this->GetEdgeArray(edgeId);
}

//------------------------------------------------------------------------------
vtkIdType vtkPentagonalPrism::GetFacePoints(vtkIdType faceId, const vtkIdType*& pts)
{
  assert(faceId < vtkPentagonalPrism::NumberOfFaces && "faceId too large");
  pts = this->GetFaceArray(faceId);
  return NumberOfPointsInFace[faceId];
}

//------------------------------------------------------------------------------
void vtkPentagonalPrism::GetEdgeToAdjacentFaces(vtkIdType edgeId, const vtkIdType*& pts)
{
  assert(edgeId < vtkPentagonalPrism::NumberOfEdges && "edgeId too large");
  pts = EdgeToAdjacentFaces[edgeId];
}

//------------------------------------------------------------------------------
vtkIdType vtkPentagonalPrism::GetFaceToAdjacentFaces(vtkIdType faceId, const vtkIdType*& faceIds)
{
  assert(faceId < vtkPentagonalPrism::NumberOfFaces && "faceId too large");
  faceIds = FaceToAdjacentFaces[faceId];
  return NumberOfPointsInFace[faceId];
}

//------------------------------------------------------------------------------
vtkIdType vtkPentagonalPrism::GetPointToIncidentEdges(vtkIdType pointId, const vtkIdType*& edgeIds)
{
  assert(pointId < vtkPentagonalPrism::NumberOfPoints && "pointId too large");
  edgeIds = PointToIncidentEdges[pointId];
  return vtkPentagonalPrism::MaximumValence;
}

//------------------------------------------------------------------------------
vtkIdType vtkPentagonalPrism::GetPointToIncidentFaces(vtkIdType pointId, const vtkIdType*& faceIds)
{
  assert(pointId < vtkPentagonalPrism::NumberOfPoints && "pointId too large");
  faceIds = PointToIncidentFaces[pointId];
  return vtkPentagonalPrism::MaximumValence;
}

//------------------------------------------------------------------------------
vtkIdType vtkPentagonalPrism::GetPointToOneRingPoints(vtkIdType pointId, const vtkIdType*& pts)
{
  assert(pointId < vtkPentagonalPrism::NumberOfPoints && "pointId too large");
  pts = PointToOneRingPoints[pointId];
  return vtkPentagonalPrism::MaximumValence;
}

//------------------------------------------------------------------------------
//
// Intersect prism Faces against line. Each prism face is a quadrilateral.
//
int vtkPentagonalPrism::IntersectWithLine(const double p1[3], const double p2[3], double tol,
  double& t, double x[3], double pcoords[3], int& subId)
{
  int intersection = 0;
  double pt1[3], pt2[3], pt3[3], pt4[3], pt5[3];
  double tTemp;
  double pc[3], xTemp[3], dist2, weights[10];
  int faceNum;

  t = VTK_DOUBLE_MAX;

  // first intersect the penta Faces
  for (faceNum = 0; faceNum < 2; faceNum++)
  {
    this->Points->GetPoint(Faces[faceNum][0], pt1);
    this->Points->GetPoint(Faces[faceNum][1], pt2);
    this->Points->GetPoint(Faces[faceNum][2], pt3);
    this->Points->GetPoint(Faces[faceNum][3], pt4);
    this->Points->GetPoint(Faces[faceNum][4], pt5);

    this->Quad->Points->SetPoint(0, pt1);
    this->Quad->Points->SetPoint(1, pt2);
    this->Quad->Points->SetPoint(2, pt3);
    this->Quad->Points->SetPoint(3, pt4);

    this->Triangle->Points->SetPoint(0, pt4);
    this->Triangle->Points->SetPoint(1, pt5);
    this->Triangle->Points->SetPoint(2, pt1);

    if (this->Quad->IntersectWithLine(p1, p2, tol, tTemp, xTemp, pc, subId) ||
      this->Triangle->IntersectWithLine(p1, p2, tol, tTemp, xTemp, pc, subId))
    {
      intersection = 1;
      if (tTemp < t)
      {
        t = tTemp;
        x[0] = xTemp[0];
        x[1] = xTemp[1];
        x[2] = xTemp[2];
        switch (faceNum)
        {
          case 0:
            pcoords[0] = pc[0];
            pcoords[1] = pc[1];
            pcoords[2] = 0.0;
            break;

          case 1:
            pcoords[0] = pc[0];
            pcoords[1] = pc[1];
            pcoords[2] = 1.0;
            break;
        }
      }
    }
  }

  // now intersect the _5_ quad Faces
  for (faceNum = 2; faceNum < 5; faceNum++)
  {
    this->Points->GetPoint(Faces[faceNum][0], pt1);
    this->Points->GetPoint(Faces[faceNum][1], pt2);
    this->Points->GetPoint(Faces[faceNum][2], pt3);
    this->Points->GetPoint(Faces[faceNum][3], pt4);

    this->Quad->Points->SetPoint(0, pt1);
    this->Quad->Points->SetPoint(1, pt2);
    this->Quad->Points->SetPoint(2, pt3);
    this->Quad->Points->SetPoint(3, pt4);

    if (this->Quad->IntersectWithLine(p1, p2, tol, tTemp, xTemp, pc, subId))
    {
      intersection = 1;
      if (tTemp < t)
      {
        t = tTemp;
        x[0] = xTemp[0];
        x[1] = xTemp[1];
        x[2] = xTemp[2];
        this->EvaluatePosition(x, xTemp, subId, pcoords, dist2, weights);
      }
    }
  }

  return intersection;
}

//------------------------------------------------------------------------------
int vtkPentagonalPrism::TriangulateLocalIds(int vtkNotUsed(index), vtkIdList* ptIds)
{
  // Create 8 tetrahedron. This might not be the minimum, but it is a simple solution.
  // The Pentagonal Prism is divided in one hexa and one wedge.
  // The first five tetra are for the hexahedron
  // The last three tetra are for the wedge
  ptIds->SetNumberOfIds(32);
  constexpr vtkIdType localPtIds[8][4] = { { 0, 1, 3, 5 }, { 1, 5, 6, 7 }, { 1, 5, 7, 3 },
    { 1, 3, 7, 2 }, { 3, 7, 8, 5 }, { 0, 4, 5, 3 }, { 3, 5, 8, 9 }, { 3, 4, 5, 9 } };
  std::copy_n(&localPtIds[0][0], 32, ptIds->begin());
  return 1;
}

//------------------------------------------------------------------------------
//
// Compute derivatives in x-y-z directions. Use chain rule in combination
// with interpolation function derivatives.
//
void vtkPentagonalPrism::Derivatives(
  int vtkNotUsed(subId), const double pcoords[3], const double* values, int dim, double* derivs)
{
  double *jI[3], j0[3], j1[3], j2[3];
  double functionDerivs[30], sum[3];

  // compute inverse Jacobian and interpolation function derivatives
  jI[0] = j0;
  jI[1] = j1;
  jI[2] = j2;
  this->JacobianInverse(pcoords, jI, functionDerivs);

  // now compute derivates of values provided
  for (int k = 0; k < dim; k++) // loop over values per point
  {
    sum[0] = sum[1] = sum[2] = 0.0;
    for (int i = 0; i < 10; i++) // loop over interp. function derivatives
    {
      sum[0] += functionDerivs[i] * values[dim * i + k];
      sum[1] += functionDerivs[10 + i] * values[dim * i + k];
      sum[2] += functionDerivs[20 + i] * values[dim * i + k];
    }
    for (int j = 0; j < 3; j++) // loop over derivative directions
    {
      derivs[3 * k + j] = sum[0] * jI[j][0] + sum[1] * jI[j][1] + sum[2] * jI[j][2];
    }
  }
}
//------------------------------------------------------------------------------
// Given parametric coordinates compute inverse Jacobian transformation
// matrix. Returns 9 elements of 3x3 inverse Jacobian plus interpolation
// function derivatives.
void vtkPentagonalPrism::JacobianInverse(
  const double pcoords[3], double** inverse, double derivs[30])
{
  double *m[3], m0[3], m1[3], m2[3];
  double x[3];

  // compute interpolation function derivatives
  this->InterpolationDerivs(pcoords, derivs);

  // create Jacobian matrix
  m[0] = m0;
  m[1] = m1;
  m[2] = m2;
  for (int i = 0; i < 3; i++) // initialize matrix
  {
    m0[i] = m1[i] = m2[i] = 0.0;
  }

  for (int j = 0; j < 10; j++)
  {
    this->Points->GetPoint(j, x);
    for (int i = 0; i < 3; i++)
    {
      m0[i] += x[i] * derivs[j];
      m1[i] += x[i] * derivs[10 + j];
      m2[i] += x[i] * derivs[20 + j];
    }
  }

  // now find the inverse
  if (vtkMath::InvertMatrix(m, inverse, 3) == 0)
  {
    vtkErrorMacro(<< "Jacobian inverse not found");
    return;
  }
}

//------------------------------------------------------------------------------
double* vtkPentagonalPrism::GetParametricCoords()
{
  return ParametricCoords;
}

//------------------------------------------------------------------------------
void vtkPentagonalPrism::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Line:\n";
  this->Line->PrintSelf(os, indent.GetNextIndent());
  os << indent << "Quad:\n";
  this->Quad->PrintSelf(os, indent.GetNextIndent());
  os << indent << "Polygon:\n";
  this->Polygon->PrintSelf(os, indent.GetNextIndent());
}
VTK_ABI_NAMESPACE_END
