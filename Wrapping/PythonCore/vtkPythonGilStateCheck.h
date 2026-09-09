// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#ifndef vtkPythonGilStateCheck_h
#define vtkPythonGilStateCheck_h

#include <Python.h>
#if defined(_WIN32) && defined(Py_LIMITED_API)
#include "vtkWindows.h"
#endif

#if defined(Py_LIMITED_API) && !defined(_WIN32)
// PyGILState_Check is exported by CPython but is not in the stable ABI.
extern "C" int PyGILState_Check(void);
#endif

#if defined(_WIN32) && defined(Py_LIMITED_API)
static inline int vtkPythonGilStateCheck()
{
  // python3.dll forwards stable functions to the running interpreter's DLL,
  // but does not export PyGILState_Check. Resolve that documented CPython API
  // from the module containing the resolved Py_IsInitialized function, without
  // naming or loading a particular Python minor's DLL. This preserves the
  // nonblocking ownership check for nested For() and native worker threads.
  // PyGILState_Check is outside the stable ABI: the same wheel must be tested
  // on every supported minor; this is not a promise about future CPython APIs.
  using CheckFunction = int (*)();
  static const CheckFunction check = []() -> CheckFunction
  {
    HMODULE interpreter = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCSTR>(&Py_IsInitialized), &interpreter))
    {
      auto function = reinterpret_cast<CheckFunction>(
        GetProcAddress(interpreter, "PyGILState_Check"));
      if (function != nullptr)
      {
        return function;
      }
    }
    // Continuing without an ownership check risks releasing an unowned GIL
    // or deadlocking the worker join. Do not silently disable the hook.
    Py_FatalError("cvista requires the running CPython DLL to export PyGILState_Check");
    return nullptr;
  }();
  return check();
}
#else
static inline int vtkPythonGilStateCheck()
{
  return PyGILState_Check();
}
#endif

#endif
