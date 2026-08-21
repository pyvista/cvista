// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkJoystickFlyOut.h"

#include "vtkObjectFactory.h"

VTK_ABI_NAMESPACE_BEGIN

vtkStandardNewMacro(vtkJoystickFlyOut);

//-------------------------------------------------------------------------
vtkJoystickFlyOut::vtkJoystickFlyOut()
{
  this->In = 0;
}

//-------------------------------------------------------------------------
vtkJoystickFlyOut::~vtkJoystickFlyOut() = default;

//-------------------------------------------------------------------------
void vtkJoystickFlyOut::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

VTK_ABI_NAMESPACE_END
