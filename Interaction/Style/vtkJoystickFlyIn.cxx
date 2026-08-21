// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkJoystickFlyIn.h"

#include "vtkObjectFactory.h"

VTK_ABI_NAMESPACE_BEGIN

vtkStandardNewMacro(vtkJoystickFlyIn);

//-------------------------------------------------------------------------
vtkJoystickFlyIn::vtkJoystickFlyIn()
{
  this->In = 1;
}

//-------------------------------------------------------------------------
vtkJoystickFlyIn::~vtkJoystickFlyIn() = default;

//-------------------------------------------------------------------------
void vtkJoystickFlyIn::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

VTK_ABI_NAMESPACE_END
