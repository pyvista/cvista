// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkWrapPythonEnum.h"

#include "vtkWrap.h"
#include "vtkWrapText.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NOLINTBEGIN(bugprone-unsafe-functions)

/* -------------------------------------------------------------------- */
/* check whether an enum type will be wrapped */
int vtkWrapPython_IsEnumWrapped(const HierarchyInfo* hinfo, const char* enumname)
{
  int rval = 0;
  const HierarchyEntry* entry;

  if (hinfo && enumname)
  {
    entry = vtkParseHierarchy_FindEntry(hinfo, enumname);
    if (entry && entry->IsEnum && !vtkParseHierarchy_GetProperty(entry, "WRAPEXCLUDE"))
    {
      rval = 1;
    }
  }

  return rval;
}

/* -------------------------------------------------------------------- */
/* find and mark all enum parameters by setting IsEnum=1 */
void vtkWrapPython_MarkAllEnums(NamespaceInfo* contents, const HierarchyInfo* hinfo)
{
  FunctionInfo* currentFunction;
  int i, j, n, m, ii, nn;
  ClassInfo* data;
  ValueInfo* val;

  nn = contents->NumberOfClasses;
  for (ii = 0; ii < nn; ii++)
  {
    data = contents->Classes[ii];
    n = data->NumberOfFunctions;
    for (i = 0; i < n; i++)
    {
      currentFunction = data->Functions[i];
      if (!currentFunction->IsExcluded && currentFunction->Access == VTK_ACCESS_PUBLIC)
      {
        /* we start with the return value */
        val = currentFunction->ReturnValue;
        m = vtkWrap_CountWrappedParameters(currentFunction);

        /* the -1 is for the return value */
        for (j = (val ? -1 : 0); j < m; j++)
        {
          if (j >= 0)
          {
            val = currentFunction->Parameters[j];
          }

          if (vtkWrap_IsEnumMember(data, val) || vtkWrapPython_IsEnumWrapped(hinfo, val->Class))
          {
            val->IsEnum = 1;
          }
        }
      }
    }
  }
}

/* -------------------------------------------------------------------- */
/* generate a wrapped enum type (no anonymous enums, only named enums) */
void vtkWrapPython_AddEnumType(FILE* fp, const char* indent, const char* dictvar,
  const char* objvar, const char* scope, EnumInfo* cls)
{
  const ValueInfo* val;
  int j;

  if (cls->IsDeprecated)
  {
    fprintf(fp, "  /* Deprecated %s */\n", (cls->DeprecatedReason ? cls->DeprecatedReason : ""));
  }

  /* abi3: build the heap enum type from its spec and stash the pointer (the
   * deref macro makes &Py..._Type resolve to it); the default build readies the
   * static type in place. */
  fprintf(fp,
    "#if defined(Py_LIMITED_API)\n"
    "%sPy%s%s%s_TypePtr = (PyTypeObject*)PyType_FromSpec(&Py%s%s%s_Spec);\n"
    "#else\n"
    "%sPyType_Ready(&Py%s%s%s_Type);\n"
    "#endif\n",
    indent, (scope ? scope : ""), (scope ? "_" : ""), cls->Name, (scope ? scope : ""),
    (scope ? "_" : ""), cls->Name, indent, (scope ? scope : ""), (scope ? "_" : ""), cls->Name);

  if (cls->NumberOfConstants)
  {
    fprintf(fp,
      "%s// members of %s%s%s\n"
      "%s{\n"
      "%s  PyObject *enumval;\n"
      "%s  PyObject *enumdict = PyDict_New();\n"
      /* default build: enumdict IS the type's dict; abi3 merges it in below */
      "#if !defined(Py_LIMITED_API)\n"
      "%s  Py%s%s%s_Type.tp_dict = enumdict;\n"
      "#endif\n"
      "\n",
      indent, (scope ? scope : ""), (scope ? "::" : ""), cls->Name, indent, indent, indent, indent,
      (scope ? scope : ""), (scope ? "_" : ""), cls->Name);

    fprintf(fp,
      "%s  typedef %s%s%s cxx_enum_type;\n"
      "%s  static const struct {\n"
      "%s    const char *name; cxx_enum_type value;\n"
      "%s  } constants[%d] = {\n",
      indent, (scope ? scope : ""), (scope ? "::" : ""), cls->Name, indent, indent, indent,
      cls->NumberOfConstants);

    for (j = 0; j < cls->NumberOfConstants; j++)
    {
      val = cls->Constants[j];
      fprintf(fp, "%s    { \"%s%s\", cxx_enum_type::%s },%s\n", indent, val->Name,
        (vtkWrapText_IsPythonKeyword(val->Name) ? "_" : ""), val->Name,
        ((val->Attributes & VTK_PARSE_DEPRECATED) ? " /* deprecated */" : ""));
    }

    fprintf(fp,
      "%s  };\n"
      "\n",
      indent);

    fprintf(fp,
      "%s  for (int c = 0; c < %d; c++)\n"
      "%s  {\n"
      "%s    enumval = Py%s%s%s_FromEnum(constants[c].value);\n"
      "%s    if (enumval)\n"
      "%s    {\n"
      "%s      PyDict_SetItemString(enumdict, constants[c].name, enumval);\n"
      "%s      Py_DECREF(enumval);\n"
      "%s    }\n"
      "%s  }\n",
      indent, cls->NumberOfConstants, indent, indent, (scope ? scope : ""), (scope ? "_" : ""),
      cls->Name, indent, indent, indent, indent, indent, indent);

    /* abi3: copy the assembled members into the heap type's dict, then release
     * the temporary (the default build handed ownership to tp_dict above). */
    fprintf(fp,
      "#if defined(Py_LIMITED_API)\n"
      "%s  vtkPythonType_MergeIntoTypeDict(&Py%s%s%s_Type, enumdict);\n"
      "%s  Py_DECREF(enumdict);\n"
      "#endif\n",
      indent, (scope ? scope : ""), (scope ? "_" : ""), cls->Name, indent);

    fprintf(fp,
      "%s}\n"
      "\n",
      indent);
  }

  fprintf(fp,
    "%sPyVTKEnum_Add(&Py%s%s%s_Type, \"%s%s%s\");\n"
    "\n",
    indent, (scope ? scope : ""), (scope ? "_" : ""), cls->Name, (scope ? scope : ""),
    (scope ? "." : ""), cls->Name);
  fprintf(fp,
    "%s%s = (PyObject *)&Py%s%s%s_Type;\n"
    "%sif (PyDict_SetItemString(%s, \"%s\", %s) != 0)\n"
    "%s{\n"
    "%s  Py_DECREF(%s);\n"
    "%s}\n",
    indent, objvar, (scope ? scope : ""), (scope ? "_" : ""), cls->Name, indent, dictvar, cls->Name,
    objvar, indent, indent, objvar, indent);
}

/* -------------------------------------------------------------------- */
/* write out an enum type object */
void vtkWrapPython_GenerateEnumType(
  FILE* fp, const char* module, const char* classname, const EnumInfo* data)
{
  char enumname[512];
  char tpname[512];

  if (classname)
  {
    /* join with "_" for identifier, and with "." for type name */
    snprintf(enumname, sizeof(enumname), "%.200s_%.200s", classname, data->Name);
    snprintf(tpname, sizeof(tpname), "%.200s.%.200s", classname, data->Name);
  }
  else
  {
    snprintf(enumname, sizeof(enumname), "%.200s", data->Name);
    snprintf(tpname, sizeof(tpname), "%.200s", data->Name);
  }

  /* generate all functions and protocols needed for the type */

  /* abi3: a static PyTypeObject is illegal under the limited API; emit a
   * PyType_Spec mirroring the static enum type below. The enum is an int
   * subclass: Py_tp_base = &PyLong_Type (address-of an opaque global is OK),
   * basicsize 0 inherits PyLong's layout, tp_free = PyObject_Del, and the
   * DISALLOW_INSTANTIATION flag matches the >=3.10 static form. The heap type is
   * built at runtime by the abi3 branch of vtkWrapPython_AddEnumType. */
  fprintf(fp,
    "#if defined(Py_LIMITED_API)\n"
    "static PyType_Slot Py%s_Slots[] = {\n"
    "  { Py_tp_base, (void*)&PyLong_Type },\n"
    "  { Py_tp_free, (void*)PyObject_Del },\n"
    "  { 0, nullptr }\n"
    "};\n\n"
    "static PyType_Spec Py%s_Spec = {\n"
    "  PYTHON_PACKAGE_SCOPE \"%s.%s\",\n"
    "  0, 0,\n"
    "  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,\n"
    "  Py%s_Slots\n"
    "};\n"
    /* runtime pointer to the heap enum type + a deref macro so every existing
     * "&Py%s_Type" reference stays byte-identical in form (&(*ptr) == ptr). */
    "static PyTypeObject* Py%s_TypePtr = nullptr;\n"
    "#define Py%s_Type (*Py%s_TypePtr)\n"
    "#else\n",
    enumname, enumname, module, tpname, enumname, enumname, enumname, enumname, enumname);

  /* generate the TypeObject */
  fprintf(fp,
    "#ifdef VTK_PYTHON_NEEDS_DEPRECATION_WARNING_SUPPRESSION\n"
    "#pragma GCC diagnostic ignored \"-Wdeprecated-declarations\"\n"
    "#endif\n"
    "\n"
    "static PyTypeObject Py%s_Type = {\n"
    "  PyVarObject_HEAD_INIT(&PyType_Type, 0)\n"
    "  PYTHON_PACKAGE_SCOPE \"%s.%s\", // tp_name\n"
    "  sizeof(PyLongObject), // tp_basicsize\n"
    "  0, // tp_itemsize\n"
    "  nullptr, // tp_dealloc\n"
    "#if PY_VERSION_HEX >= 0x03080000\n"
    "  0, // tp_vectorcall_offset\n"
    "#else\n"
    "  nullptr, // tp_print\n"
    "#endif\n"
    "  nullptr, // tp_getattr\n"
    "  nullptr, // tp_setattr\n"
    "  nullptr, // tp_compare\n"
    "  nullptr, // tp_repr\n",
    enumname, module, tpname);

  fprintf(fp,
    "  nullptr, // tp_as_number\n"
    "  nullptr, // tp_as_sequence\n"
    "  nullptr, // tp_as_mapping\n"
    "  nullptr, // tp_hash\n"
    "  nullptr, // tp_call\n"
    "  nullptr, // tp_str\n"
    "  nullptr, // tp_getattro\n"
    "  nullptr, // tp_setattro\n"
    "  nullptr, // tp_as_buffer\n"
    "  Py_TPFLAGS_DEFAULT\n"
    "#if PY_VERSION_HEX >= 0x030A0000\n"
    "    | Py_TPFLAGS_DISALLOW_INSTANTIATION\n"
    "#endif\n"
    "  , // tp_flags\n"
    "  nullptr, // tp_doc\n"
    "  nullptr, // tp_traverse\n"
    "  nullptr, // tp_clear\n"
    "  nullptr, // tp_richcompare\n"
    "  0, // tp_weaklistoffset\n");

  fprintf(fp,
    "  nullptr, // tp_iter\n"
    "  nullptr, // tp_iternext\n"
    "  nullptr, // tp_methods\n"
    "  nullptr, // tp_members\n"
    "  nullptr, // tp_getset\n"
    "  &PyLong_Type, // tp_base\n"
    "  nullptr, // tp_dict\n"
    "  nullptr, // tp_descr_get\n"
    "  nullptr, // tp_descr_set\n"
    "  0, // tp_dictoffset\n"
    "  nullptr, // tp_init\n"
    "  nullptr, // tp_alloc\n"
    "  nullptr, // tp_new\n"
    "  PyObject_Del, // tp_free\n"
    "  nullptr, // tp_is_gc\n");

  /* fields set by python itself */
  fprintf(fp,
    "  nullptr, // tp_bases\n"
    "  nullptr, // tp_mro\n"
    "  nullptr, // tp_cache\n"
    "  nullptr, // tp_subclasses\n"
    "  nullptr, // tp_weaklist\n");

  /* internal struct members */
  fprintf(fp,
    "  VTK_WRAP_PYTHON_SUPPRESS_UNINITIALIZED\n"
    "};\n"
    "#endif // Py_LIMITED_API\n"
    "\n");

  /* conversion method: construct from enum value */
  fprintf(fp,
    "template<class T>\n"
    "static PyObject *Py%s_FromEnum(T val)\n"
    "{\n"
    "  return PyVTKEnum_New(&Py%s_Type, static_cast<int>(val));\n"
    "}\n"
    "\n",
    enumname, enumname);
}

/* generate code that adds all public enum types to a python dict */
void vtkWrapPython_AddPublicEnumTypes(
  FILE* fp, const char* indent, const char* dictvar, const char* objvar, NamespaceInfo* data)
{
  char text[1024];
  const char* pythonname = data->Name;
  int i;

  if (data->Name)
  {
    /* convert C++ class names to a python-friendly format */
    vtkWrapText_PythonName(data->Name, text);
    pythonname = text;
  }

  for (i = 0; i < data->NumberOfEnums; i++)
  {
    if (!data->Enums[i]->IsExcluded && data->Enums[i]->Access == VTK_ACCESS_PUBLIC)
    {
      vtkWrapPython_AddEnumType(fp, indent, dictvar, objvar, pythonname, data->Enums[i]);
      fprintf(fp, "\n");
    }
  }
}

// NOLINTEND(bugprone-unsafe-functions)
