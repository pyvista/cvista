// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkWebXRInteractorStyle.h"

#include "vtkObjectFactory.h"
#include "vtkWebXRRenderWindowInteractor.h"

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkWebXRInteractorStyle);

//------------------------------------------------------------------------------
void vtkWebXRInteractorStyle::SetupActions(vtkRenderWindowInteractor* iren)
{
  vtkWebXRRenderWindowInteractor* oiren = vtkWebXRRenderWindowInteractor::SafeDownCast(iren);

  if (oiren)
  {
    oiren->AddAction("elevation", vtkCommand::Elevation3DEvent);
    oiren->AddAction("movement", vtkCommand::ViewerMovement3DEvent);
    oiren->AddAction("nextcamerapose", vtkCommand::NextPose3DEvent);
    oiren->AddAction("positionprop", vtkCommand::PositionProp3DEvent);
    oiren->AddAction("showmenu", vtkCommand::Menu3DEvent);
    oiren->AddAction("startelevation", vtkCommand::Elevation3DEvent);
    oiren->AddAction("startmovement", vtkCommand::ViewerMovement3DEvent);
    oiren->AddAction("triggeraction", vtkCommand::Select3DEvent);
  }
}
VTK_ABI_NAMESPACE_END
