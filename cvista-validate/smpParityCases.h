// SPDX-License-Identifier: BSD-3-Clause
// Registry of the algorithms exercised by the SMP parallel-vs-serial validator.
// Each Case wires one filter onto a shared, deterministically-built input and
// exposes it as a vtkAlgorithm whose output is compared byte-exact across
// backends. The set spans every SMP risk class (per-element / reduction /
// isosurface / point-merge) and every input topology (image, polydata, UG).
#ifndef smpParityCases_h
#define smpParityCases_h

#include <vtkSmartPointer.h>

#include <functional>
#include <string>
#include <vector>

class vtkAlgorithm;
class vtkImageData;
class vtkPolyData;
class vtkUnstructuredGrid;

namespace smpparity
{

struct Inputs
{
  vtkSmartPointer<vtkImageData> image;       // vtkRTAnalyticSource wavelet, "RTData"
  vtkSmartPointer<vtkPolyData> poly;         // sphere + scalars/vectors/normals/cell data
  vtkSmartPointer<vtkUnstructuredGrid> ugrid; // thresholded wavelet + cell data
};

// Build the shared inputs. MUST be called with the SMP backend forced serial so
// the inputs themselves are deterministic (some builders use parallel filters).
Inputs BuildInputs();

enum class Risk
{
  PerElement, // out[i] = f(in[i]); no cross-iteration state (claimed always exact)
  Reduce,     // accumulation / averaging (FP sum-order sensitivity)
  Iso,        // isosurface / cut / clip: output IDs from parallel prefix sums
  Merge       // point-merge / hashing / labeling: highest divergence risk
};
const char* RiskName(Risk r);

struct Case
{
  std::string name;
  std::string module;
  Risk risk;
  // Build a fresh, fully-configured algorithm reading the shared inputs; its
  // GetOutputDataObject(0) after Update() is the result to compare.
  std::function<vtkSmartPointer<vtkAlgorithm>(const Inputs&)> make;
  // When true, this filter's threaded path emits the same geometry in a
  // thread-dependent ORDER (parallel cut/contour extraction), so parallel output
  // is compared to serial ORDER-INSENSITIVELY (same point/cell set) rather than
  // byte-exact. It must still be byte-exact run-to-run (deterministic); the
  // driver checks that separately. Default false = strict byte-exact vs serial.
  bool orderRelaxed = false;
};

std::vector<Case> RegisterCases();

} // namespace smpparity

#endif
