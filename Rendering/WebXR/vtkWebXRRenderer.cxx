// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkWebXRRenderer.h"

#include "vtkCommand.h"
#include "vtkObjectFactory.h"
#include "vtkWebXRCamera.h"

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkWebXRRenderer);

//------------------------------------------------------------------------------
vtkWebXRRenderer::vtkWebXRRenderer() = default;

//------------------------------------------------------------------------------
vtkCamera* vtkWebXRRenderer::MakeCamera()
{
  vtkCamera* cam = vtkWebXRCamera::New();
  this->InvokeEvent(vtkCommand::CreateCameraEvent, cam);
  return cam;
}

//------------------------------------------------------------------------------
void vtkWebXRRenderer::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
VTK_ABI_NAMESPACE_END
