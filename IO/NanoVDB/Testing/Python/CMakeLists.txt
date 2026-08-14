find_program(NanoVDB_VDB_PRINT
  nanovdb_print nanovdb_print.exe
  HINTS "${OpenVDB_CMAKE_PATH}/../../../bin"
  DOC "nanovdb_print executable from the NanoVDB library"
  )
mark_as_advanced(NanoVDB_VDB_PRINT)

if (NanoVDB_VDB_PRINT)
  set(SaveNanoVDBFile_ARGS "--NANOVDB_PRINT_EXE" "${NanoVDB_VDB_PRINT}")
endif()

set(PY_TESTS
  NO_VALID NO_RT
  SaveNanoVDBFile.py
  )

vtk_add_test_python(
  ${PY_TESTS}
  )
