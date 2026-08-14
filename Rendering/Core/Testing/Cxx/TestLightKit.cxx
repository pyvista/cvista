// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
// Test vtkLightKit rendering with the Stanford dragon.
//
// This test dynamically cycles through the lights in a vtkLightKit to
// highlight the effect of each one.

#include "vtkActor.h"
#include "vtkCamera.h"
#include "vtkCommand.h"
#include "vtkLightKit.h"
#include "vtkNew.h"
#include "vtkPLYReader.h"
#include "vtkPolyDataMapper.h"
#include "vtkPolyDataNormals.h"
#include "vtkProperty.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkTextActor.h"

#include "vtkRegressionTestImage.h"
#include "vtkTestUtilities.h"

#include "vtkTextMapper.h"
#include "vtkTextProperty.h"
#include <string>
#include <vector>

namespace
{
struct LightStep
{
  const char* Name;
  double KeyIntensity;
  double FillRatio;
  double BackRatio;
  double HeadRatio;
};

class vtkTimerCallback : public vtkCommand
{
public:
  static vtkTimerCallback* New()
  {
    vtkTimerCallback* cb = new vtkTimerCallback;
    cb->TimerCount = 0;
    return cb;
  }

  vtkTypeMacro(vtkTimerCallback, vtkCommand);

  void Execute(vtkObject* caller, unsigned long eventId, void* vtkNotUsed(callData)) override
  {
    if (vtkCommand::TimerEvent == eventId)
    {
      ++this->TimerCount;
    }
    if (this->TimerCount % 2 == 0) // Change light every 2 seconds
    {
      this->CurrentStep = (this->CurrentStep + 1) % this->LightSteps.size();
      this->LightKit->SetKeyLightIntensity(this->LightSteps[this->CurrentStep].KeyIntensity);
      this->LightKit->SetKeyToFillRatio(this->LightSteps[this->CurrentStep].FillRatio);
      this->LightKit->SetKeyToBackRatio(this->LightSteps[this->CurrentStep].BackRatio);
      this->LightKit->SetKeyToHeadRatio(this->LightSteps[this->CurrentStep].HeadRatio);

      std::string text = "Highlighting: ";
      text += this->LightSteps[this->CurrentStep].Name;
      this->TextActor->SetInput(text.c_str());

      vtkRenderWindowInteractor* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
      if (iren)
      {
        iren->GetRenderWindow()->Render();
      }
    }
  }

public:
  vtkLightKit* LightKit;
  vtkTextActor* TextActor;
  int TimerCount;
  int CurrentStep = 0;
  std::vector<LightStep> LightSteps;

private:
  vtkTimerCallback() = default;
};
}

//------------------------------------------------------------------------------
int TestLightKit(int argc, char* argv[])
{
  const char* fileName = vtkTestUtilities::ExpandDataFileName(argc, argv, "Data/dragon.ply");
  vtkNew<vtkPLYReader> reader;
  reader->SetFileName(fileName);
  reader->Update();
  delete[] fileName;

  vtkNew<vtkPolyDataNormals> normals;
  normals->SetInputConnection(reader->GetOutputPort());
  normals->ComputePointNormalsOn();
  normals->SplittingOff();

  vtkNew<vtkPolyDataMapper> mapper;
  mapper->SetInputConnection(normals->GetOutputPort());

  vtkNew<vtkActor> actor;
  actor->SetMapper(mapper);
  actor->GetProperty()->SetDiffuseColor(0.8, 0.7, 0.6);
  actor->GetProperty()->SetSpecular(0.4);
  actor->GetProperty()->SetDiffuse(0.8);
  actor->GetProperty()->SetAmbient(0.1);
  actor->GetProperty()->SetSpecularPower(40.0);

  vtkNew<vtkLightKit> lightKit;

  vtkNew<vtkRenderer> renderer;
  renderer->SetBackground(0.1, 0.1, 0.15);
  renderer->AddActor(actor);
  renderer->AutomaticLightCreationOff();
  lightKit->AddLightsToRenderer(renderer);
  renderer->ResetCamera();
  vtkCamera* camera = renderer->GetActiveCamera();
  camera->Azimuth(-40);
  camera->Elevation(5);
  camera->Dolly(1.5);
  renderer->ResetCameraClippingRange();

  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetSize(600, 600);
  renderWindow->AddRenderer(renderer);

  vtkNew<vtkRenderWindowInteractor> iren;
  iren->SetRenderWindow(renderWindow);

  // Setup text to show current light
  vtkNew<vtkTextActor> textActor;
  textActor->GetTextProperty()->SetFontSize(24);
  textActor->GetTextProperty()->SetJustificationToCentered();
  textActor->GetTextProperty()->SetVerticalJustificationToTop();
  textActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedDisplay();
  textActor->GetPositionCoordinate()->SetValue(0.5, 0.95);

  // Setup timer callback
  vtkNew<vtkTimerCallback> observer;
  observer->LightKit = lightKit;
  observer->TextActor = textActor;
  observer->LightSteps = {
    { "All Lights", 0.75, 3.0, 3.5, 3.0 },
    { "Key Light", 1.0, 1000.0, 1000.0, 1000.0 },
    { "Fill Light", 0.75, 1.0, 1000.0, 1000.0 },
    { "Back Light", 0.75, 1000.0, 1.0, 1000.0 },
    { "Head Light", 0.75, 1000.0, 1000.0, 1.0 },
  };

  // Set initial state
  lightKit->SetKeyLightIntensity(observer->LightSteps[0].KeyIntensity);
  lightKit->SetKeyToFillRatio(observer->LightSteps[0].FillRatio);
  lightKit->SetKeyToBackRatio(observer->LightSteps[0].BackRatio);
  lightKit->SetKeyToHeadRatio(observer->LightSteps[0].HeadRatio);
  lightKit->SetFillLightWarmth(0.1);
  lightKit->SetKeyLightWarmth(0.9);
  lightKit->SetBackLightWarmth(0.5);
  lightKit->SetHeadLightWarmth(0.05);
  std::string initialText = "Highlighting: ";
  initialText += observer->LightSteps[0].Name;
  textActor->SetInput(initialText.c_str());

  renderWindow->Render();
  iren->Initialize();

  int retVal = vtkRegressionTestImage(renderWindow);
  if (retVal == vtkRegressionTester::DO_INTERACTOR)
  {
    renderer->AddViewProp(textActor);

    iren->AddObserver(vtkCommand::TimerEvent, observer);
    iren->CreateRepeatingTimer(1000); // 1 second timer

    iren->Start();
  }

  return !retVal;
}
