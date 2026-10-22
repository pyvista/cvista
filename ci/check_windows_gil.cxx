// SPDX-License-Identifier: BSD-3-Clause
// Link against python3.lib before the expensive native wheel build. Exercise
// the actual ownership helper with attached, detached, and native threads.
#include "vtkPythonGilStateCheck.h"

#include <cstdlib>
#include <thread>

int main()
{
  Py_Initialize();
  if (!vtkPythonGilStateCheck())
  {
    return EXIT_FAILURE;
  }
  bool nativeHeldGil = true;
  std::thread attached([&nativeHeldGil]() { nativeHeldGil = vtkPythonGilStateCheck() != 0; });
  attached.join();
  if (nativeHeldGil)
  {
    return EXIT_FAILURE;
  }
  auto state = PyEval_SaveThread();
  if (vtkPythonGilStateCheck())
  {
    return EXIT_FAILURE;
  }
  nativeHeldGil = true;
  std::thread native([&nativeHeldGil]() { nativeHeldGil = vtkPythonGilStateCheck() != 0; });
  native.join();
  PyEval_RestoreThread(state);
  if (nativeHeldGil || !vtkPythonGilStateCheck())
  {
    return EXIT_FAILURE;
  }
  Py_Finalize();
  return EXIT_SUCCESS;
}
