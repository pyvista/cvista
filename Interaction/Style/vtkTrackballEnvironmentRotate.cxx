// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkTrackballEnvironmentRotate.h"

#include "vtkMath.h"
#include "vtkMatrix3x3.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"

#include <array>

VTK_ABI_NAMESPACE_BEGIN

//-------------------------------------------------------------------------
vtkStandardNewMacro(vtkTrackballEnvironmentRotate);

//-------------------------------------------------------------------------
void vtkTrackballEnvironmentRotate::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//-------------------------------------------------------------------------
void vtkTrackballEnvironmentRotate::EnvironmentRotate(
  vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  const int dx = rwi->GetEventPosition()[0] - rwi->GetLastEventPosition()[0];
  const int sizeX = ren->GetRenderWindow()->GetSize()[0];

  vtkNew<vtkMatrix3x3> mat;

  std::array<double, 3> up, right, front;
  ren->GetEnvironmentUp(up.data());
  ren->GetEnvironmentRight(right.data());

  vtkMath::Cross(right, up, front);
  for (int i = 0; i < 3; i++)
  {
    mat->SetElement(i, 0, right[i]);
    mat->SetElement(i, 1, up[i]);
    mat->SetElement(i, 2, front[i]);
  }

  const double angle = (dx / static_cast<double>(sizeX)) * this->GetMouseMotionFactor();

  const double c = std::cos(angle);
  const double s = std::sin(angle);
  const double t = 1.0 - c;

  vtkNew<vtkMatrix3x3> rot;

  rot->SetElement(0, 0, t * up[0] * up[0] + c);
  rot->SetElement(0, 1, t * up[0] * up[1] - up[2] * s);
  rot->SetElement(0, 2, t * up[0] * up[2] + up[1] * s);

  rot->SetElement(1, 0, t * up[0] * up[1] + up[2] * s);
  rot->SetElement(1, 1, t * up[1] * up[1] + c);
  rot->SetElement(1, 2, t * up[1] * up[2] - up[0] * s);

  rot->SetElement(2, 0, t * up[0] * up[2] - up[1] * s);
  rot->SetElement(2, 1, t * up[1] * up[2] + up[0] * s);
  rot->SetElement(2, 2, t * up[2] * up[2] + c);

  vtkMatrix3x3::Multiply3x3(rot, mat, mat);

  // update environment orientation
  ren->SetEnvironmentRight(mat->GetElement(0, 0), mat->GetElement(1, 0), mat->GetElement(2, 0));
}

//-------------------------------------------------------------------------
void vtkTrackballEnvironmentRotate::OnMouseMove(
  int vtkNotUsed(x), int vtkNotUsed(y), vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  if (ren == nullptr)
  {
    return;
  }

  // Update environment orientation
  this->EnvironmentRotate(ren, rwi);

  rwi->Render();
}

VTK_ABI_NAMESPACE_END
