// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "PyVTKEnum.h"
#include "vtkABINamespace.h"
#include "vtkPythonTypeAccess.h"
#include "vtkPythonUtil.h"

#include <cstddef>

//------------------------------------------------------------------------------
// C API

//------------------------------------------------------------------------------
// Add a wrapped enum type
PyTypeObject* PyVTKEnum_Add(PyTypeObject* pytype, const char* name)
{
#if PY_VERSION_HEX < 0x030A0000
  // do not allow direct instantiation
  pytype->tp_new = nullptr;
#endif
  vtkPythonUtil::AddEnumToMap(pytype, name);
  return pytype;
}

//------------------------------------------------------------------------------
PyObject* PyVTKEnum_New(PyTypeObject* pytype, int val)
{
  // our enums are subtypes of Python's int() type
  PyObject* args = Py_BuildValue("(i)", val);
#if defined(Py_LIMITED_API)
  // PyLong_Type is opaque under the limited API; reach its tp_new via the stable
  // slot accessor (address-of the global is permitted, member access is not).
  newfunc longNew = vtkPythonType_GetNew(&PyLong_Type);
  PyObject* obj = longNew(pytype, args, nullptr);
#else
  PyObject* obj = PyLong_Type.tp_new(pytype, args, nullptr);
#endif
  Py_DECREF(args);
  return obj;
}
