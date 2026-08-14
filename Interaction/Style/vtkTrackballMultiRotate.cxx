// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright 2009 Sandia Corporation
// SPDX-License-Identifier: LicenseRef-BSD-3-Clause-Sandia-USGov

#include "vtkTrackballMultiRotate.h"

#include "vtkObjectFactory.h"
#include "vtkRenderer.h"
#include "vtkTrackballRoll.h"
#include "vtkTrackballRotate.h"

#include <algorithm>
#include <array>
#include <cmath>

VTK_ABI_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkTrackballMultiRotate);

//-----------------------------------------------------------------------------
vtkTrackballMultiRotate::vtkTrackballMultiRotate()
{
  this->RotateManipulator = vtkTrackballRotate::New();
  this->RollManipulator = vtkTrackballRoll::New();
  this->CurrentManipulator = nullptr;
}

vtkTrackballMultiRotate::~vtkTrackballMultiRotate()
{
  this->CurrentManipulator = nullptr;
  this->RotateManipulator->Delete();
  this->RollManipulator->Delete();
}

void vtkTrackballMultiRotate::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//-----------------------------------------------------------------------------
void vtkTrackballMultiRotate::OnButtonDown(
  int x, int y, vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  int* viewSize;
  viewSize = ren->GetSize();
  std::array<double, 2> viewCenter;
  viewCenter[0] = 0.5 * viewSize[0];
  viewCenter[1] = 0.5 * viewSize[1];
  double rotateRadius = 0.9 * (std::max<double>(viewCenter[0], viewCenter[1]));
  double dist2 = std::pow(viewCenter[0] - x, 2) + std::pow(viewCenter[1] - y, 2);

  if (rotateRadius * rotateRadius > dist2)
  {
    this->CurrentManipulator = this->RotateManipulator;
  }
  else
  {
    this->CurrentManipulator = this->RollManipulator;
  }

  this->CurrentManipulator->SetMouseButton(this->GetMouseButton());
  this->CurrentManipulator->SetShift(this->GetShift());
  this->CurrentManipulator->SetControl(this->GetControl());
  this->CurrentManipulator->SetCenterOfRotation(this->GetCenterOfRotation());

  this->CurrentManipulator->OnButtonDown(x, y, ren, rwi);
}

//-----------------------------------------------------------------------------
void vtkTrackballMultiRotate::OnButtonUp(
  int x, int y, vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  if (this->CurrentManipulator)
  {
    this->CurrentManipulator->OnButtonUp(x, y, ren, rwi);
  }
}

//-----------------------------------------------------------------------------
void vtkTrackballMultiRotate::OnMouseMove(
  int x, int y, vtkRenderer* ren, vtkRenderWindowInteractor* rwi)
{
  if (this->CurrentManipulator)
  {
    this->CurrentManipulator->OnMouseMove(x, y, ren, rwi);
  }
}

VTK_ABI_NAMESPACE_END
