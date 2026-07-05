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
class vtkTable;
class vtkUnstructuredGrid;

namespace smpparity
{

struct Inputs
{
  vtkSmartPointer<vtkImageData> image;        // 3D vtkRTAnalyticSource wavelet, "RTData"
  vtkSmartPointer<vtkImageData> image2d;      // 2D single-slice wavelet
  vtkSmartPointer<vtkImageData> labelImage;   // 3D integer-label image ("labels")
  vtkSmartPointer<vtkImageData> labelImage2d; // 2D integer-label image
  vtkSmartPointer<vtkPolyData> poly;          // sphere + scalars/vectors/cell data
  vtkSmartPointer<vtkPolyData> polyNT;        // sphere + point Normals + TCoords
  vtkSmartPointer<vtkPolyData> poly2;         // translated sphere (2nd surface)
  vtkSmartPointer<vtkPolyData> cloud;         // point cloud (verts) + "scalars"
  vtkSmartPointer<vtkPolyData> cloudPlanar;   // coplanar point cloud (z=0)
  vtkSmartPointer<vtkUnstructuredGrid> ugrid; // thresholded wavelet + cell data
  vtkSmartPointer<vtkTable> table;            // numeric columns A,B
  vtkSmartPointer<vtkTable> table2;           // numeric columns A,B (2nd)
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
  // When true, this filter is KNOWN to be nondeterministic under threading for a
  // documented reason (knownIssueReason). The driver still runs it and reports the
  // divergence + magnitude, but does NOT count it as a gate failure -- so the CI
  // gate keeps protecting the deterministic majority while transparently tracking
  // the known exceptions. Two distinct kinds live here: (a) inherently random
  // filters whose SERIAL output is already nondeterministic (an unseeded RNG), and
  // (b) genuine threading bugs (a data race) recorded pending a fix.
  bool knownIssue = false;
  std::string knownIssueReason;
};

std::vector<Case> RegisterCases();

} // namespace smpparity

#endif
