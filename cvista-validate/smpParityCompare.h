// SPDX-License-Identifier: BSD-3-Clause
// Byte-exact comparison of two VTK data objects, for the SMP parallel-vs-serial
// parity validator. "Byte-exact" = every stored array value compares bit-for-bit
// (memcmp of each value, so NaN==NaN and -0.0 != +0.0 behave as an on-disk
// comparison would), plus identical topology and attribute structure.
#ifndef smpParityCompare_h
#define smpParityCompare_h

#include <string>

class vtkDataObject;

namespace smpparity
{
// Returns an empty string when a and b are byte-identical outputs; otherwise a
// short human-readable description of the first difference found.
std::string CompareDataObjects(vtkDataObject* a, vtkDataObject* b);

// Order-INSENSITIVE equality: returns "" when a and b describe the same geometry
// as a SET -- identical multiset of points (with point data) and cells (with
// cell data), regardless of the order points/cells are stored in. Used for
// filters whose threaded path emits the same result in a thread-dependent order
// (e.g. parallel cut/contour extraction). A difference here means the geometry
// itself differs, not merely its ordering.
std::string CompareGeometrySet(vtkDataObject* a, vtkDataObject* b);
}

#endif
