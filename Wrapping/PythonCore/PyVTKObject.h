// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/*-----------------------------------------------------------------------
  The PyVTKObject was created in Oct 2000 by David Gobbi for VTK 3.2.
  It was rewritten in Jul 2015 to wrap VTK classes as python type objects.
-----------------------------------------------------------------------*/

#ifndef PyVTKObject_h
#define PyVTKObject_h

#include "vtkABINamespace.h"
#include "vtkPython.h"
#include "vtkSystemIncludes.h"
#include "vtkWrappingPythonCoreModule.h" // For export macro

VTK_ABI_NAMESPACE_BEGIN
class vtkObjectBase;
VTK_ABI_NAMESPACE_END
typedef vtkObjectBase* (*vtknewfunc)();

// Flags for special properties or features
#define VTK_PYTHON_IGNORE_UNREGISTER 1 // block Register/UnRegister calls

// This class is used for defining new VTK wrapped classes.
// It contains information such as the methods and docstring, as well
// as extra info that can't easily be stored in the PyTypeObject.
VTK_ABI_NAMESPACE_BEGIN
class VTKWRAPPINGPYTHONCORE_EXPORT PyVTKClass
{
public:
  PyVTKClass()
    : py_type(nullptr)
    , py_methods(nullptr)
    , vtk_name(nullptr)
    , vtk_new(nullptr)
  {
  }

  PyVTKClass(
    PyTypeObject* typeobj, PyMethodDef* methods, const char* classname, vtknewfunc constructor);

  PyTypeObject* py_type;
  PyMethodDef* py_methods;
  const char* vtk_name; // the name returned by GetClassName()
  vtknewfunc vtk_new;   // creates a C++ instance of classtype
};

// This is the VTK/Python 'object,' it contains the python object header
// plus a pointer to the associated vtkObjectBase and PyVTKClass.
struct PyVTKObject
{
  PyObject_HEAD
  PyObject* vtk_dict;           // each object has its own dict
  PyObject* vtk_weakreflist;    // list of weak references via python
  PyVTKClass* vtk_class;        // information about the class
  vtkObjectBase* vtk_ptr;       // pointer to the C++ object
  Py_ssize_t* vtk_buffer;       // ndims, shape, strides for Py_buffer
  unsigned long* vtk_observers; // used to find our observers
  unsigned int vtk_flags;       // flags (see list above)
};

// This struct holds the getter and setter for a property
struct PyVTKGetSet
{
  PyCFunction get;
  PyCFunction set;
};
VTK_ABI_NAMESPACE_END

extern VTKWRAPPINGPYTHONCORE_EXPORT PyGetSetDef PyVTKObject_GetSet[];
#if defined(Py_LIMITED_API)
// abi3 only: the getset table for vtkObjectBase's spec — same as PyVTKObject_GetSet
// plus the "__dict__" descriptor, which under the limited API can only be declared
// on the root type and is inherited by subclasses (see PyVTKObject.cxx).
extern VTKWRAPPINGPYTHONCORE_EXPORT PyGetSetDef PyVTKObject_BaseGetSet[];
#endif
#if !defined(Py_LIMITED_API)
// PyBufferProcs is not exposed by the limited API; under abi3 the buffer
// protocol is wired through the Py_bf_getbuffer / Py_bf_releasebuffer type-spec
// slots instead (see PyVTKObject.cxx and the generator's PyType_Spec emission).
extern VTKWRAPPINGPYTHONCORE_EXPORT PyBufferProcs PyVTKObject_AsBuffer;
#else
// abi3: the buffer slot functions have external linkage so a generated
// PyType_Spec can reference them directly as Py_bf_getbuffer/Py_bf_releasebuffer.
extern "C"
{
  VTKWRAPPINGPYTHONCORE_EXPORT
  int PyVTKObject_AsBuffer_GetBuffer(PyObject* obj, Py_buffer* view, int flags);
  VTKWRAPPINGPYTHONCORE_EXPORT
  void PyVTKObject_AsBuffer_ReleaseBuffer(PyObject* obj, Py_buffer* view);
}
#endif

extern "C"
{
#if defined(Py_LIMITED_API)
  // abi3: the type cannot be a static PyTypeObject, so the generator passes the
  // PyType_Spec plus the runtime-resolved base; the type is built here with
  // PyType_FromSpec (a heap type) and its method dict populated via the
  // SetDictItem accessor. See PyVTKObject.cxx for the contract.
  VTKWRAPPINGPYTHONCORE_EXPORT
  PyTypeObject* PyVTKClass_Add(PyType_Spec* spec, PyTypeObject* base, PyMethodDef* methods,
    const char* classname, vtknewfunc constructor);
#else
  VTKWRAPPINGPYTHONCORE_EXPORT
  PyTypeObject* PyVTKClass_Add(
    PyTypeObject* pytype, PyMethodDef* methods, const char* classname, vtknewfunc constructor);
#endif

  VTKWRAPPINGPYTHONCORE_EXPORT
  void PyVTKClass_AddCombinedGetSetDefinitions(PyTypeObject* pytype, PyGetSetDef* getsets);

  VTKWRAPPINGPYTHONCORE_EXPORT
  int PyVTKObject_Check(PyObject* obj);

  VTKWRAPPINGPYTHONCORE_EXPORT
  PyObject* PyVTKObject_FromPointer(PyTypeObject* vtkclass, PyObject* pydict, vtkObjectBase* ptr);

  VTKWRAPPINGPYTHONCORE_EXPORT
  vtkObjectBase* PyVTKObject_GetObject(PyObject* obj);

  VTKWRAPPINGPYTHONCORE_EXPORT
  void PyVTKObject_AddObserver(PyObject* obj, unsigned long id);

  VTKWRAPPINGPYTHONCORE_EXPORT
  void PyVTKObject_SetFlag(PyObject* obj, unsigned int flag, int val);

  VTKWRAPPINGPYTHONCORE_EXPORT
  unsigned int PyVTKObject_GetFlags(PyObject* obj);

  VTKWRAPPINGPYTHONCORE_EXPORT
  PyObject* PyVTKObject_Repr(PyObject* op);

  VTKWRAPPINGPYTHONCORE_EXPORT
  PyObject* PyVTKObject_String(PyObject* op);

  VTKWRAPPINGPYTHONCORE_EXPORT
  int PyVTKObject_Traverse(PyObject* o, visitproc visit, void* arg);

  VTKWRAPPINGPYTHONCORE_EXPORT
  PyObject* PyVTKObject_New(PyTypeObject*, PyObject* args, PyObject* kwds);

  VTKWRAPPINGPYTHONCORE_EXPORT
  int PyVTKObject_Init(PyObject* obj, PyObject* args, PyObject* kwds);

  VTKWRAPPINGPYTHONCORE_EXPORT
  void PyVTKObject_Delete(PyObject* op);

  VTKWRAPPINGPYTHONCORE_EXPORT
  PyObject* PyVTKObject_GetProperty(PyObject* op, void* methods);

  VTKWRAPPINGPYTHONCORE_EXPORT
  int PyVTKObject_SetProperty(PyObject* op, PyObject* value, void* methods);

  VTKWRAPPINGPYTHONCORE_EXPORT
  int PyVTKObject_SetPropertyMulti(PyObject* op, PyObject* value, void* methods);
}

#endif
/* VTK-HeaderTest-Exclude: PyVTKObject.h */
