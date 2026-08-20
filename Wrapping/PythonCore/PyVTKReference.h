// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/*-----------------------------------------------------------------------
  The PyVTKReference was created in Sep 2010 by David Gobbi.

  This class is a proxy for python int and float, it allows these objects
  to be passed to VTK methods that require a ref to a numeric type.
-----------------------------------------------------------------------*/

#ifndef PyVTKReference_h
#define PyVTKReference_h

#include "vtkABINamespace.h"
#include "vtkPython.h"
#include "vtkSystemIncludes.h"
#include "vtkWrappingPythonCoreModule.h" // For export macro

// The PyVTKReference is a wrapper around a PyObject of
// type int or float.
struct PyVTKReference
{
  PyObject_HEAD
  PyObject* value;
};

// Under Py_LIMITED_API (abi3) a PyTypeObject is opaque and cannot be a static
// object; the type is built at runtime via PyType_FromSpec (a heap type) and
// referenced through a pointer. The `#define PyVTKXxx_Type (*ptr)` shim keeps
// every existing `&PyVTKXxx_Type` / `Py_TYPE(o) == &PyVTKXxx_Type` use-site
// byte-identical in the default build and correct (resolving to the pointer)
// under abi3. See vtkPythonTypeAccess.h.
#if defined(Py_LIMITED_API)
extern PyTypeObject* PyVTKReference_TypePtr;
extern PyTypeObject* PyVTKNumberReference_TypePtr;
extern PyTypeObject* PyVTKStringReference_TypePtr;
extern PyTypeObject* PyVTKTupleReference_TypePtr;
#define PyVTKReference_Type (*PyVTKReference_TypePtr)
#define PyVTKNumberReference_Type (*PyVTKNumberReference_TypePtr)
#define PyVTKStringReference_Type (*PyVTKStringReference_TypePtr)
#define PyVTKTupleReference_Type (*PyVTKTupleReference_TypePtr)
#else
extern PyTypeObject PyVTKReference_Type;
extern PyTypeObject PyVTKNumberReference_Type;
extern PyTypeObject PyVTKStringReference_Type;
extern PyTypeObject PyVTKTupleReference_Type;
#endif

#define PyVTKReference_Check(obj) PyObject_TypeCheck(obj, &PyVTKReference_Type)

#if defined(Py_LIMITED_API)
// abi3: build the four reference heap types (idempotent). 0 on success, -1 on
// failure. Must be called before any reference type is used.
int PyVTKReference_BuildTypes();
#endif

extern "C"
{
  // Set the value held by a mutable object.  It steals the reference
  // of the provided value.  Only float, long, and int are allowed.
  // A return value of -1 indicates than an error occurred.
  int PyVTKReference_SetValue(PyObject* self, PyObject* val);

  // Get the value held by a mutable object.  A borrowed reference
  // is returned.
  PyObject* PyVTKReference_GetValue(PyObject* self);
}

#endif
